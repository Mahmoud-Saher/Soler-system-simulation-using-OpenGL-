#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glut.h>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <vector>
#include <string>
#include <stb_image.h>
#include <irrKlang.h>
using namespace std;
using namespace glm;
using namespace irrklang;
using namespace ImGui;

const GLuint WIDTH = 1920, HEIGHT = 1080;
float planetSpeed = 1.0f;
float sunRotationSpeed = 50.0f;
float sunBrightness = 1.0f;
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;
float Volume = 0.1;
int selectedPlanet = -1;
bool sunPulseEnabled = true;
bool showOrbits = true;
bool showStars = true;
bool firstMouse = true;
bool freeCameraEnabled = false;
GLuint sunTexture;
GLuint saturnRingTexture;
ISoundEngine* soundEngine = createIrrKlangDevice();

GLuint loadTexture(const char* filename) {
    int width, height, channels;
    unsigned char* image = stbi_load(filename, &width, &height, &channels, 0);

    GLenum format = GL_RGB;
    if (channels == 4) format = GL_RGBA;

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(image);
    return texture;
}

struct Camera {
    vec3 pos = vec3(0.0f, 0.0f, 50.0f);
    vec3 front = vec3(0.0f, 0.0f, -1.0f);
    vec3 up = vec3(0.0f, 1.0f, 0.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    float speed = 0.05f;
    float sensitivity = 0.03f;
    float AngleX = 0.0f;
    float AngleY = 90.0f;
    float Distance = 50.0f;

    void updateDirection(float xoffset, float yoffset) {
        yaw += xoffset * sensitivity;
        pitch -= yoffset * sensitivity;

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        vec3 dir;
        dir.x = cos(radians(yaw)) * cos(radians(pitch));
        dir.y = sin(radians(pitch));
        dir.z = sin(radians(yaw)) * cos(radians(pitch));
        front = normalize(dir);
    }
    mat4 getViewMatrix() {
        return lookAt(pos, pos + front, up);
    }
};
Camera camera;

struct Planet {
    string name;
    float size;
    float orbitRadius;
    float spinSpeed;
    float orbitSpeed;
    GLuint texture;
    bool hasRings;
    float ringInnerRadius;
    float ringOuterRadius;
    float mass;
    float temperature;
    int moonCount;
    string composition;
    string funFact;

    Planet(const string& name, float size, float orbitRadius,
        float spinSpeed, float orbitSpeed, const char* texturePath,
        bool hasRings = false, float ringInner = 0, float ringOuter = 0,
        float mass = 1.0f, float temp = 288.0f, int moons = 0,
        string comp = " ", string fact = " ")
        : name(name), size(size), orbitRadius(orbitRadius),
        spinSpeed(spinSpeed), orbitSpeed(orbitSpeed),
        hasRings(hasRings), ringInnerRadius(ringInner), ringOuterRadius(ringOuter),
        mass(mass), temperature(temp), moonCount(moons),
        composition(comp), funFact(fact) {
        texture = loadTexture(texturePath);
    }
};
vector<Planet> planets;

struct Star {
    float x, y, z;
    float phase;
    float r, g, b;
};
vector<Star> stars;

void setupProjection(GLfloat width, GLfloat height) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, width / height, 0.1f, 500.0f);
    glMatrixMode(GL_MODELVIEW);
}

void setCamera() {
    glLoadIdentity();
    mat4 view = freeCameraEnabled ? camera.getViewMatrix() :
        lookAt(
            vec3(camera.Distance * sin(radians(camera.AngleY)) * cos(radians(camera.AngleX)), camera.Distance * cos(radians(camera.AngleY)), camera.Distance * sin(radians(camera.AngleY)) * sin(radians(camera.AngleX))),
            vec3(0.0f),
            vec3(0.0f, 1.0f, 0.0f));
    glLoadMatrixf(value_ptr(view));
}

void initializePlanets() {
    planets.emplace_back("Mercury", 0.3f, 5.0f, 100.0f, 150.0f, "textures/mercury.jpg", false, 0, 0, 0.055f, 440.0f, 0, "70% Metallic, 30% Silicate", "A day on Mercury (sunrise to sunrise) lasts 176 Earth days");
    planets.emplace_back("Venus", 0.5f, 7.5f, 80.0f, 120.0f, "textures/venus.jpg", false, 0, 0, 0.815f, 737.0f, 0, "Rocky, CO2 Atmosphere", "Rotates backwards compared to most planets");
    planets.emplace_back("Earth", 0.6f, 10.0f, 90.0f, 100.0f, "textures/earth.jpg", false, 0, 0, 1.0f, 288.0f, 1, "Nitrogen-Oxygen Atmosphere", "Only known planet with liquid water on surface");
    planets.emplace_back("Mars", 0.4f, 12.5f, 70.0f, 80.0f, "textures/mars.jpg", false, 0, 0, 0.107f, 210.0f, 2, "Iron Oxide Surface", "Home to the tallest volcano in the solar system (Olympus Mons)");
    planets.emplace_back("Jupiter", 1.0f, 16.0f, 40.0f, 50.0f, "textures/jupiter.jpg", false, 0, 0, 317.8f, 165.0f, 79, "Hydrogen-Helium Gas Giant", "Has a persistent storm (Great Red Spot) larger than Earth");
    planets.emplace_back("Saturn", 0.9f, 20.0f, 30.0f, 40.0f, "textures/saturn.jpg", true, 0.9f * 1.2f, 0.9f * 2.0f, 95.2f, 134.0f, 82, "Hydrogen-Helium with Ice", "Would float if placed in a large enough body of water");
    planets.emplace_back("Uranus", 0.8f, 25.0f, 20.0f, 30.0f, "textures/uranus.jpg", false, 0.0f, 0.0f, 14.5f, 76.0f, 27, "Ice Giant (Water, Ammonia, Methane)", "Rotates on its side (98° axial tilt)");
    planets.emplace_back("Neptune", 0.7f, 30.0f, 15.0f, 20.0f, "textures/neptune.jpg", false, 0.0f, 0.0f, 17.1f, 72.0f, 14, "Ice Giant (Water, Ammonia, Methane)", "Has the strongest winds in the solar system (2,100 km/h)");
}

void generateStars(int count) {
    float time = glfwGetTime();
    stars.clear();
    srand(static_cast<unsigned int>(time));
    for (int i = 0; i < count; ++i) {
        stars.push_back({
            (rand() / (float)RAND_MAX - 0.5f) * 200.0f,
            (rand() / (float)RAND_MAX - 0.5f) * 200.0f,
            (rand() / (float)RAND_MAX - 0.5f) * 200.0f,
            rand() / (float)RAND_MAX * 6.28f,
            rand() / (float)RAND_MAX,
            rand() / (float)RAND_MAX,
            rand() / (float)RAND_MAX
        });
    }
}

void drawStarfield() {
    float time = glfwGetTime();
    if (!showStars) return;
    glDisable(GL_LIGHTING);
    glPointSize(1.5f);
    glBegin(GL_POINTS);
    for (auto& star : stars) {
        float brightness = 0.5f + 0.5f * sinf(time + star.phase);
        glColor3f(star.r * brightness, star.g * brightness, star.b * brightness);
        glVertex3f(star.x, star.y, star.z);
    }
    glEnd();
    glEnable(GL_LIGHTING);
}

void drawOrbit() {
    if (!showOrbits) return;

    glDisable(GL_LIGHTING);
    glColor3f(0.3f, 0.3f, 0.3f);

    for (auto& planet : planets) {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i <= 100; ++i) {
            float theta = i * 2.0f * 3.14f / 100;
            glVertex3f(cos(theta) * planet.orbitRadius, 0.0f, sin(theta) * planet.orbitRadius);
        }
        glEnd();
    }
    glEnable(GL_LIGHTING);
}

void renderText3D(float x, float y, float z, const string& text) {
    glDisable(GL_LIGHTING);
    glColor3f(1, 1, 1);
    glRasterPos3f(x, y, z);
    for (char c : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    glEnable(GL_LIGHTING);
}

void setMaterial(float r, float g, float b, bool emissive = false) {
    GLfloat diff[] = { r, g, b, 1.0f };
    GLfloat amb[] = { r * 0.3f, g * 0.3f, b * 0.3f, 1.0f };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
    glMaterialfv(GL_FRONT, GL_AMBIENT, amb);

    if (emissive) {
        GLfloat emis[] = { r, g, b, 1.0f };
        glMaterialfv(GL_FRONT, GL_EMISSION, emis);
    }
    else {
        GLfloat emis[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        glMaterialfv(GL_FRONT, GL_EMISSION, emis);
    }
}

void setSunEmissiveMaterial(float brightness) {
    float r = brightness;
    float g = brightness * 0.8f;
    float b = 0.0f;

    GLfloat ambient[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat specular[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    GLfloat emission[] = { r, g, b, 1.0f };

    glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, specular);
    glMaterialfv(GL_FRONT, GL_EMISSION, emission);
}

void drawSaturnRings(float innerRadius, float outerRadius) {
    glPushMatrix();
    glEnable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, saturnRingTexture);

    GLUquadricObj* ringQuad = gluNewQuadric();
    gluQuadricTexture(ringQuad, GL_TRUE);
    gluQuadricNormals(ringQuad, GLU_SMOOTH);
    gluDisk(ringQuad, innerRadius, outerRadius, 64, 1);
    gluDeleteQuadric(ringQuad);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glPopMatrix();
}

void drawPlanet(float radius) {
    static GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    gluSphere(quad, radius, 32, 32);
}

void renderPlanet() {
    float time = glfwGetTime();
    for (int i = 0; i < planets.size(); i++) {
        auto& planet = planets[i];
        glPushMatrix();
        glRotatef(time * planet.orbitSpeed * planetSpeed, 0, 1, 0);
        glTranslatef(planet.orbitRadius, 0, 0);
        glRotatef(time * planet.spinSpeed, 0, 1, 0);

        if (i == selectedPlanet) {
            float pulse = 0.1f * sin(time * 5) + 1.0f;
            glDisable(GL_LIGHTING);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDepthMask(GL_FALSE);

            glColor4f(1, 0.8, 0.3, 0.3f);
            glutSolidSphere(planet.size * 1.3f * pulse, 32, 32);

            glColor4f(1, 0.8, 0.3, 0.7f);
            glutSolidSphere(planet.size * 1.1f * pulse, 32, 32);

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glEnable(GL_LIGHTING);
        }

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, planet.texture);
        setMaterial(1.0f, 1.0f, 1.0f);
        drawPlanet(planet.size);
        glDisable(GL_TEXTURE_2D);

        renderText3D(0, planet.size + 0.2f, 0, planet.name);

        if (planet.hasRings) {
            glPushMatrix();
            glRotatef(time * planet.spinSpeed, 1, 0, 0);
            drawSaturnRings(planet.ringInnerRadius, planet.ringOuterRadius);
            glPopMatrix();
        }
        glPopMatrix();
    }
}

void renderSun() {
    float time = glfwGetTime();
    glPushMatrix();
    glRotatef(time * sunRotationSpeed, 0.0f, 1.0f, 0.0f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, sunTexture);

    setSunEmissiveMaterial(sunBrightness);

    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluSphere(quad, 2, 50, 50);
    gluDeleteQuadric(quad);

    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
}

void processFreeCameraInput(GLFWwindow* window) {
    if (!freeCameraEnabled) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.AngleY -= 1.0f * camera.speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.AngleY += 1.0f * camera.speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.AngleX -= 1.0f * camera.speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.AngleX += 1.0f * camera.speed;
    }
    else {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.pos += camera.speed * camera.front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.pos -= camera.speed * camera.front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.pos -= normalize(cross(camera.front, camera.up)) * camera.speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.pos += normalize(cross(camera.front, camera.up)) * camera.speed;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) camera.pos += camera.up * camera.speed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camera.pos -= camera.up * camera.speed;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) camera.speed = 0.1f;
    else camera.speed = 0.05f;
}

void keyCallback(GLFWwindow* win, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_8) {
            selectedPlanet = key - GLFW_KEY_1;
            if (selectedPlanet == 2) soundEngine->stopAllSounds(), soundEngine->play2D("sounds/sound.wav");
            else soundEngine->stopAllSounds(), soundEngine->play2D("sounds/select.mp3");
        }
        if (key == GLFW_KEY_9) selectedPlanet = -1;
        if (key == GLFW_KEY_R) freeCameraEnabled = false;
        if (key == GLFW_KEY_F) freeCameraEnabled = true;
    }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (!freeCameraEnabled) return;

    if (firstMouse) {
        lastX = float(xpos);
        lastY = float(ypos);
        firstMouse = false;
    }

    float xoffset = float(xpos - lastX);
    float yoffset = float(ypos - lastY);
    lastX = float(xpos);
    lastY = float(ypos);

    camera.updateDirection(xoffset, yoffset);
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.Distance -= yoffset * 1.0f;
    if (camera.Distance < 5.0f) camera.Distance = 5.0f;
    if (camera.Distance > 200.0f) camera.Distance = 200.0f;
}

void framebufferSizeCallback(GLFWwindow* win, int width, int height) {
    glViewport(0, 0, width, height);
    setupProjection(width, height);
}

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Solar System", NULL, NULL);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwMakeContextCurrent(window);
    glewInit();
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glEnable(GL_DEPTH_TEST);

    sunTexture = loadTexture("textures/sun.jpg");
    saturnRingTexture = loadTexture("textures/saturn_rings.png");
    generateStars(3000);
    initializePlanets();

    IMGUI_CHECKVERSION();
    CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    StyleColorsDark();
    setupProjection(WIDTH, HEIGHT);
    glEnable(GL_TEXTURE_2D);

    while (!glfwWindowShouldClose(window)) {
        float time = glfwGetTime();
        soundEngine->setSoundVolume(Volume);

        if (freeCameraEnabled) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        else glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        if (!freeCameraEnabled) firstMouse = true;
        if (sunPulseEnabled) sunBrightness = 1 + 0.2f * sin(time * 2.0f);
        else sunBrightness = sunBrightness;

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        processFreeCameraInput(window);

        setCamera();
        drawStarfield();
        drawOrbit();
        renderSun();
        renderPlanet();

        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        GLfloat lightPos[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        GLfloat lightAmbient[] = { 0.1f, 0.1f, 0.1f, 1.0f };
        GLfloat lightDiffuse[] = { sunBrightness, sunBrightness, sunBrightness, 1.0f };
        GLfloat lightSpecular[] = { sunBrightness, sunBrightness, sunBrightness, 1.0f };

        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        NewFrame();
        GetIO().FontGlobalScale = 1.5f;

        Begin("Control Panel");
        if (BeginTabBar("Tabs")) {
            if (BeginTabItem("Planet Controls")) {
                SliderFloat("Planet Speed", &planetSpeed, 0.0f, 5.0f);
                for (auto& planet : planets) {
                    SliderFloat((planet.name + " Size").c_str(), &planet.size, 0.1f, 2.0f);
                    SliderFloat((planet.name + " Orbit Radius").c_str(), &planet.orbitRadius, 1.0f, 40.0f);
                    SliderFloat((planet.name + " Spin Speed").c_str(), &planet.spinSpeed, 0.0f, 200.0f);
                    SliderFloat((planet.name + " Orbit Speed").c_str(), &planet.orbitSpeed, 0.0f, 200.0f);
                    if (planet.hasRings) {
                        SliderFloat((planet.name + " Ring Inner").c_str(), &planet.ringInnerRadius, 0.1f, planet.size * 3.0f);
                        SliderFloat((planet.name + " Ring Outer").c_str(), &planet.ringOuterRadius, planet.ringInnerRadius + 0.1f, planet.size * 4.0f);
                    }
                }
                EndTabItem();
            }
            if (BeginTabItem("Sun & Lighting")) {
                Checkbox("Sun pulse effect", &sunPulseEnabled);
                SliderFloat("Sun Brightness", &sunBrightness, 0.0f, 5.0f);
                SliderFloat("Sun Speed", &sunRotationSpeed, 0.0f, 50.0f);
                EndTabItem();
            }
            if (BeginTabItem("Starfield & Orbits")) {
                Checkbox("Show Orbits", &showOrbits);
                Checkbox("Show Stars", &showStars);
                SliderFloat("Volume", &Volume, 0.0, 1.0f);
                EndTabItem();
            }
            EndTabBar();
        }
        End();

        if (Begin("Planet Information")) {
            if (selectedPlanet != -1) {
                Planet& p = planets[selectedPlanet];

                TextColored(ImVec4(1, 1, 0, 1), "%s", p.name.c_str());
                Columns(2, "planetinfo");
                Text("Mass:"); NextColumn(); Text("%.3f Earths", p.mass); NextColumn();
                Text("Temperature:"); NextColumn(); Text("%.1f K", p.temperature); NextColumn();
                Text("Moons:"); NextColumn(); Text("%d", p.moonCount); NextColumn();
                Text("Composition:"); NextColumn(); Text("%s", p.composition.c_str()); NextColumn();
                Columns(1);
                Separator();
                TextWrapped("Fun Fact: %s", p.funFact.c_str());
            }
            else {
                Text("No planet selected");
                Text("Press 1-8 to select or 9 to deselect a planet");
            }
        }
        End();

        Render();
        ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}