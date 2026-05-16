#include <GL/glut.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define WINDOW_WIDTH   1024
#define WINDOW_HEIGHT  768
#define PLATFORM_SIZE  60
#define RENDER_DISTANCE 30
#define MOVE_SPEED     4.0f
#define SPRINT_SPEED   6.5f
#define MOUSE_SENS     0.2f
#define PLAYER_HEIGHT  1.62f
#define GRAVITY        20.0f
#define JUMP_FORCE     8.0f
#define PLAYER_RADIUS  0.3f

float camX = 0.0f, camY = PLAYER_HEIGHT, camZ = 0.0f;
float velocityY = 0.0f;
float yaw = 0.0f, pitch = 0.0f;
int keys[256];
int mouseCaptured = 0;
int isOnGround = 1;

float terrainHeight[PLATFORM_SIZE][PLATFORM_SIZE];

typedef struct {
    float r, g, b;
} Color;

Color grassTop = {0.25f, 0.75f, 0.15f};
Color grassSide = {0.50f, 0.35f, 0.10f};
Color dirtColor = {0.55f, 0.40f, 0.20f};
Color stoneColor = {0.50f, 0.50f, 0.50f};
Color sandColor = {0.85f, 0.80f, 0.50f};

float noise(int x, int y) {
    int n = x + y * 57;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

float smoothNoise(int x, int y) {
    float corners = (noise(x-1, y-1) + noise(x+1, y-1) + noise(x-1, y+1) + noise(x+1, y+1)) / 16.0f;
    float sides = (noise(x-1, y) + noise(x+1, y) + noise(x, y-1) + noise(x, y+1)) / 8.0f;
    float center = noise(x, y) / 4.0f;
    return corners + sides + center;
}

float interpolatedNoise(float x, float y) {
    int intX = (int)x;
    float fracX = x - intX;
    int intY = (int)y;
    float fracY = y - intY;
    
    float v1 = smoothNoise(intX, intY);
    float v2 = smoothNoise(intX + 1, intY);
    float v3 = smoothNoise(intX, intY + 1);
    float v4 = smoothNoise(intX + 1, intY + 1);
    
    float i1 = v1 + (v2 - v1) * fracX;
    float i2 = v3 + (v4 - v3) * fracX;
    
    return i1 + (i2 - i1) * fracY;
}

float getTerrainHeight(int x, int z) {
    float scale = 0.05f;
    float height = interpolatedNoise(x * scale, z * scale) * 4.0f;
    height += interpolatedNoise(x * scale * 2, z * scale * 2) * 2.0f;
    
    float distFromCenter = sqrtf(x*x + z*z) / (PLATFORM_SIZE/2.0f);
    if (distFromCenter > 0.7f) {
        height -= (distFromCenter - 0.7f) * 8.0f;
    }
    
    height = floorf(height);
    
    return height;
}

void generateTerrain() {
    int half = PLATFORM_SIZE / 2;
    for (int i = 0; i < PLATFORM_SIZE; i++) {
        for (int j = 0; j < PLATFORM_SIZE; j++) {
            int worldX = i - half;
            int worldZ = j - half;
            terrainHeight[i][j] = getTerrainHeight(worldX, worldZ);
        }
    }
}

int getBlockAtPosition(float x, float y, float z) {
    int half = PLATFORM_SIZE / 2;
    int gridX = (int)floorf(x + half);
    int gridZ = (int)floorf(z + half);
    
    if (gridX < 0 || gridX >= PLATFORM_SIZE || gridZ < 0 || gridZ >= PLATFORM_SIZE) {
        return 0;
    }
    
    float groundHeight = terrainHeight[gridX][gridZ];
    int blockY = (int)floorf(y);
    
    if (blockY <= groundHeight) {
        return 1;
    }
    
    return 0;
}

int checkCollision(float x, float y, float z) {
    float checkPoints[][2] = {
        {x - PLAYER_RADIUS, z - PLAYER_RADIUS},
        {x + PLAYER_RADIUS, z - PLAYER_RADIUS},
        {x - PLAYER_RADIUS, z + PLAYER_RADIUS},
        {x + PLAYER_RADIUS, z + PLAYER_RADIUS}
    };
    
    for (int i = 0; i < 4; i++) {
        if (getBlockAtPosition(checkPoints[i][0], y, checkPoints[i][1])) {
            return 1;
        }
        if (getBlockAtPosition(checkPoints[i][0], y + PLAYER_HEIGHT, checkPoints[i][1])) {
            return 1;
        }
    }
    
    return 0;
}

void initGL() {
    glClearColor(0.53f, 0.81f, 0.98f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_FLAT);
    
    GLfloat ambient[] = {0.7f, 0.7f, 0.7f, 1.0f};
    GLfloat diffuse[] = {0.9f, 0.9f, 0.8f, 1.0f};
    GLfloat lightPos[] = {0.0f, 100.0f, 0.0f, 1.0f};

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
}

void drawBlockFaceWithColor(Color top, Color side, Color bottom) {
    float s = 0.5f;
    
    glColor3f(top.r, top.g, top.b);
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-s, s, -s);
    glVertex3f(s, s, -s);
    glVertex3f(s, s, s);
    glVertex3f(-s, s, s);
    glEnd();
    
    glColor3f(bottom.r, bottom.g, bottom.b);
    glBegin(GL_QUADS);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-s, -s, s);
    glVertex3f(s, -s, s);
    glVertex3f(s, -s, -s);
    glVertex3f(-s, -s, -s);
    glEnd();
    
    glColor3f(side.r, side.g, side.b);
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(-s, -s, -s);
    glVertex3f(s, -s, -s);
    glVertex3f(s, s, -s);
    glVertex3f(-s, s, -s);
    
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(s, -s, s);
    glVertex3f(-s, -s, s);
    glVertex3f(-s, s, s);
    glVertex3f(s, s, s);
    
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-s, -s, s);
    glVertex3f(-s, -s, -s);
    glVertex3f(-s, s, -s);
    glVertex3f(-s, s, s);
    
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f(s, -s, -s);
    glVertex3f(s, -s, s);
    glVertex3f(s, s, s);
    glVertex3f(s, s, -s);
    glEnd();
}

void drawPlatform() {
    int half = PLATFORM_SIZE / 2;
    int camGridX = (int)(camX + half);
    int camGridZ = (int)(camZ + half);
    
    for (int i = 0; i < PLATFORM_SIZE; i++) {
        for (int j = 0; j < PLATFORM_SIZE; j++) {
            if (abs(i - camGridX) > RENDER_DISTANCE || abs(j - camGridZ) > RENDER_DISTANCE) {
                continue;
            }
            
            int worldX = i - half;
            int worldZ = j - half;
            float height = terrainHeight[i][j];
            
            Color topColor, sideColor;
            
            if (height < 1.0f) {
                topColor = sandColor;
                sideColor = sandColor;
            } else {
                topColor = grassTop;
                sideColor = grassSide;
            }
            
            for (int y = 0; y < height; y++) {
                glPushMatrix();
                glTranslatef(worldX + 0.5f, y + 0.5f, worldZ + 0.5f);
                
                if (y == height - 1) {
                    drawBlockFaceWithColor(topColor, sideColor, dirtColor);
                } else if (y >= height - 3) {
                    Color darkerDirt = {dirtColor.r * 0.9f, dirtColor.g * 0.9f, dirtColor.b * 0.9f};
                    drawBlockFaceWithColor(darkerDirt, darkerDirt, stoneColor);
                } else {
                    Color darkerStone = {stoneColor.r * 0.8f, stoneColor.g * 0.8f, stoneColor.b * 0.8f};
                    drawBlockFaceWithColor(darkerStone, darkerStone, stoneColor);
                }
                
                glPopMatrix();
            }
        }
    }
}

void drawCrosshair() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    
    int cx = WINDOW_WIDTH / 2;
    int cy = WINDOW_HEIGHT / 2;
    int size = 10;
    int gap = 3;
    
    glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
    glLineWidth(2.0f);
    
    glBegin(GL_LINES);
    glVertex2i(cx - size, cy);
    glVertex2i(cx - gap, cy);
    glVertex2i(cx + gap, cy);
    glVertex2i(cx + size, cy);
    glVertex2i(cx, cy - size);
    glVertex2i(cx, cy - gap);
    glVertex2i(cx, cy + gap);
    glVertex2i(cx, cy + size);
    glEnd();
    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    float lookX = camX + sinf(yaw * M_PI / 180.0f) * cosf(pitch * M_PI / 180.0f);
    float lookY = camY + sinf(pitch * M_PI / 180.0f);
    float lookZ = camZ - cosf(yaw * M_PI / 180.0f) * cosf(pitch * M_PI / 180.0f);
    
    gluLookAt(camX, camY, camZ, lookX, lookY, lookZ, 0.0f, 1.0f, 0.0f);
    
    drawPlatform();
    drawCrosshair();
    
    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(75.0, (double)w / (double)h, 0.1, PLATFORM_SIZE * 2.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboardDown(unsigned char key, int x, int y) {
    keys[key] = 1;
    if (key == 27) exit(0);
    if (key == ' ') {
        if (isOnGround) {
            velocityY = JUMP_FORCE;
            isOnGround = 0;
        }
    }
}

void keyboardUp(unsigned char key, int x, int y) {
    keys[key] = 0;
}

void mouseMove(int x, int y) {
    static int centerX = WINDOW_WIDTH / 2;
    static int centerY = WINDOW_HEIGHT / 2;
    
    if (!mouseCaptured) {
        glutWarpPointer(centerX, centerY);
        mouseCaptured = 1;
        return;
    }
    
    int dx = x - centerX;
    int dy = y - centerY;
    
    if (dx != 0 || dy != 0) {
        yaw += dx * MOUSE_SENS;
        pitch += dy * MOUSE_SENS;
        
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        
        while (yaw < 0.0f) yaw += 360.0f;
        while (yaw >= 360.0f) yaw -= 360.0f;
        
        glutWarpPointer(centerX, centerY);
    }
}

void mouseClick(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        mouseCaptured = 0;
        glutWarpPointer(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
    }
}

void update(float deltaTime) {
    float yawRad = yaw * M_PI / 180.0f;
    float forwardX = sinf(yawRad);
    float forwardZ = -cosf(yawRad);
    float rightX = cosf(yawRad);
    float rightZ = sinf(yawRad);
    
    float speed = MOVE_SPEED;
    if (keys['q'] || keys['Q']) {
        speed = SPRINT_SPEED;
    }
    
    float newX = camX;
    float newZ = camZ;
    
    if (keys['w'] || keys['W']) {
        newX += forwardX * speed * deltaTime;
        newZ += forwardZ * speed * deltaTime;
    }
    if (keys['s'] || keys['S']) {
        newX -= forwardX * speed * deltaTime;
        newZ -= forwardZ * speed * deltaTime;
    }
    if (keys['a'] || keys['A']) {
        newX -= rightX * speed * deltaTime;
        newZ -= rightZ * speed * deltaTime;
    }
    if (keys['d'] || keys['D']) {
        newX += rightX * speed * deltaTime;
        newZ += rightZ * speed * deltaTime;
    }
    
    if (!checkCollision(newX, camY, camZ)) {
        camX = newX;
    }
    if (!checkCollision(camX, camY, newZ)) {
        camZ = newZ;
    }
    
    velocityY -= GRAVITY * deltaTime;
    float newY = camY + velocityY * deltaTime;
    
    float groundY = 0.0f;
    int foundGround = 0;
    for (float testY = newY; testY >= newY - 5.0f; testY -= 0.1f) {
        if (checkCollision(camX, testY, camZ)) {
            groundY = testY + 1.0f;
            foundGround = 1;
            break;
        }
    }
    
    if (foundGround && velocityY <= 0.0f) {
        camY = groundY;
        velocityY = 0.0f;
        isOnGround = 1;
    } else if (!checkCollision(camX, newY, camZ)) {
        camY = newY;
        isOnGround = 0;
    } else {
        camY = groundY;
        velocityY = 0.0f;
        isOnGround = 1;
    }
    
    float limit = (PLATFORM_SIZE / 2.0f) - 2.0f;
    if (camX < -limit) camX = -limit;
    if (camX >  limit) camX =  limit;
    if (camZ < -limit) camZ = -limit;
    if (camZ >  limit) camZ =  limit;
}

void idle() {
    static int lastTime = 0;
    int now = glutGet(GLUT_ELAPSED_TIME);
    float deltaTime = (now - lastTime) / 1000.0f;
    lastTime = now;
    
    if (deltaTime > 0.05f) deltaTime = 0.05f;
    if (deltaTime <= 0.0f) deltaTime = 0.016f;
    
    update(deltaTime);
    glutPostRedisplay();
}

int main(int argc, char **argv) {
    srand(time(NULL));
    generateTerrain();
    
    float startHeight = getTerrainHeight(0, 0) + 1.0f;
    camY = startHeight + PLAYER_HEIGHT;
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("main");

    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutPassiveMotionFunc(mouseMove);
    glutMouseFunc(mouseClick);
    glutIdleFunc(idle);
    
    glutSetCursor(GLUT_CURSOR_NONE);
    glutWarpPointer(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);

    glutMainLoop();
    return 0;
}
