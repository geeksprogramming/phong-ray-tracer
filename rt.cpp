/////////////////////////////////////////////////////
//
// A simple ray tracer, with Phong lighting.
//
//////////////////////////////////////////////////////

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/string_cast.hpp>

#include "camera.hpp"
#include "kbui.hpp"

#include "object.hpp"
#include "triangle.hpp"
#include "sphere.hpp"
#include "hit.hpp"
#include "hitlist.hpp"
#include "tokenizer.hpp"
#include "light.hpp"
#include "material.hpp"

using namespace std;

KBUI the_ui;
Camera cam;

/////////////////////////////////////////////////////
// DECLARATIONS
////////////////////////////////////////////////////

// Forward declarations for functions in this file
void init_UI();
void setup_camera();
void check_for_resize();
void set_ray(int xDCS, int yDCS, glm::vec3& start, glm::vec3& direction);
bool first_hit(const glm::vec3& start, const glm::vec3& direction, Hit& hit);
glm::vec3 ray_color(int xDCS, int yDCS);
glm::vec3 mirror_direction(const glm::vec3& L, const glm::vec3& N);
glm::vec3 local_illumination(const glm::vec3& V, const glm::vec3& N,
                             const glm::vec3& L,
                             const Material& mat, const glm::vec3& ls);
void read_scene(const char *sceneFile);

void render();
void camera_changed();
void cam_param_changed(float);
bool get_was_window_resized();
void reset_camera(float);
void init_scene();
void mouse_button_callback( GLFWwindow* window, int button,
                            int action, int mods );
static void error_callback(int error, const char* description);
static void key_callback(GLFWwindow* window, int key,
                         int scancode, int action, int mods);
void display ();
int main(int argc, char *argv[]);

// USEFUL Flag:
// When the user clicks on a pixel, the mouse_button_callback does two things:
//   1. sets this flag.
//   2. calls ray_color() on that pixel
// This lets you check all your intersection code, for ONE ray of your choosing.
bool debugOn = false;

// FRAME BUFFER Declarations.
// The initial image is 500 x 500 x 3 bytes (3 bytes per pixel)
int winWidth  = 500;
int winHeight = 500;
GLubyte *img = NULL;   // image is allocated by check_for_resize(), not here.

// These are the camera parameters.
// The camera position and orientation:
glm::vec3 eye;
glm::vec3 lookat;
glm::vec3 vup;

// The camera's HOME parameters, used in reset_camera()
glm::vec3 eye_home(1.4, 1.6, 4.6);
glm::vec3 lookat_home(0, 0, 0);
glm::vec3 vup_home(0, 1, 0);

// The clipping frustum
float clipL = -1;
float clipR = +1;
float clipB = -1;
float clipT = +1;
float clipN =  2;

// The camera's HOME frustum, also used in reset_camera()
float clip_home[5] = {-1, +1, -1, +1, 2};

float ambient_fraction_home = 0.2; // how much of lights is ambient

vector<Object*> scene_objects; // list of objects in the scene
vector<Light> scene_lights; // list of lights in the scene
vector<Material> materials; // list of available materials

glm::vec3 ambient_light; // indirect light that shines when all lights blocked
float ambient_fraction = 0.2; // how much of lights is ambient

glm::mat4 Mvcswcs;  // the inverse of the view matrix.
HitList hits;     // list of hit records for current ray
Hit *hit_pool;     // array of available hit records.

// Used to trigger render() when camera has changed.
bool frame_buffer_stale = true;

// Rays which miss all objects have this color.
const glm::vec3 background_color(0.3, 0.4, 0.4); // dark blue

// Shadows on/off
float show_shadows = 0.0;

//====================================================================
// IMPLEMENT THE FUNCTIONS BELOW THIS LINE.
//

//////////////////////////////////////////////////////////////////////
// Compute Mvcstowcs.
// YOU MUST IMPLEMENT THIS FUNCTION.
//////////////////////////////////////////////////////////////////////
void setup_camera() {

    // Please leave this line in place.
    check_for_resize();


    // camera's x, y, z basis vectors
    glm::vec3 z = glm::normalize(eye - lookat);
    glm::vec3 x = glm::normalize(glm::cross(vup, z));
    glm::vec3 y = glm::cross(z, x);
    
    // Inverse
    Mvcswcs = glm::mat4( glm::vec4(x, 0.0), glm::vec4(y, 0.0), glm::vec4(z, 0.0), glm::vec4(eye, 1.0));
}

/////////////////////////////////////////////////////////
// Get color of a ray passing through (x,y)DCS.
// YOU MUST IMPLEMENT THIS FUNCTION.
/////////////////////////////////////////////////////////

glm::vec3 ray_color(int xDCS, int yDCS)
{
    glm::vec3 start;
    glm::vec3 direction;
    set_ray(xDCS, yDCS, start, direction);

    Hit hit;
    if (first_hit(start, direction, hit)) {
        glm::vec3 color(0.0f);
        Material mat = hit.getObject()->get_material();
      
        for (int i = 0; i < (int) scene_lights.size(); ++i) {
            glm::vec3 V = -direction;
            glm::vec3 N = hit.normal();
            glm::vec3 L = glm::normalize(scene_lights[i].get_pos() - hit.hitPoint());
            glm::vec3 Clight = scene_lights[i].get_color();
            
            // Conditions for not visibility
            // If light is below the surface (N dot L < 0)
            // If eye is below the surface (V dot N < 0)
            if (!(glm::dot(N, L) < 0.0f) && !(glm::dot(V, N) < 0.0f)) {
                color += local_illumination(V, N, L, mat, Clight);
            }
        }

        color += mat.get_ambient() * ambient_light;
        return color;
    }

    return background_color;
}

/////////////////////////////////////////////////////////
// Initialize a ray starting at (x y)DCS.
// Parameters:
// xDCS, yDCS: the pixel's coordinates
// start: the ray's starting point (you must modify this)
// direction: the ray's direction vector (you must modify this)
//
// YOU MUST IMPLEMENT THIS FUNCTION.
/////////////////////////////////////////////////////////

void set_ray(int xDCS, int yDCS, glm::vec3& start, glm::vec3& direction) {
    float dx = (clipR - clipL) / winWidth;
    float dy = (clipT - clipB) / winHeight;
    float xVCS = clipL + (xDCS + 0.5) * dx;
    float yVCS = clipB + (yDCS + 0.5) * dy;
    float zVCS = -clipN;

    glm::vec3 pWCS = Mvcswcs * glm::vec4(xVCS, yVCS, zVCS, 1.0);
    glm::vec3 vWCS = glm::normalize(pWCS - eye);

    start = pWCS;
    direction = vWCS;
}

/////////////////////////////////////////////////////////
// Find the first object hit by the ray (if any).
//
// Parameters:
// start: the ray's starting point
// direction: the ray's direction vector
// hit: if there is an intersection, store its info here
//
// Returns:
// true/false if there is/isn't an intersection.
//
// YOU MUST IMPLEMENT THIS FUNCTION.
/////////////////////////////////////////////////////////

bool first_hit(const glm::vec3& start, const glm::vec3& direction, Hit& hit) {

    hits.clear();

    for (int i = 0; i < (int)scene_objects.size(); ++i) {
        Hit h;
        if (scene_objects[i]->intersects(start, direction, h)) {
            hits.add(h);
        }
    }

    if (hits.isEmpty())
        return false;
    else {
        hit = hits.getMin();
        return true;
    }
}

///////////////////////////////////////////////////////////////////////
// Compute the reflection direction.
//
// YOU MAY HAVE TO IMPLEMENT THIS FUNCTION.
//
///////////////////////////////////////////////////////////////////////

glm::vec3 mirror_direction(const glm::vec3& L, const glm::vec3& N) {
    glm::vec3 R = 2 * glm::dot(N, L) * N - L;
    return glm::normalize(R);
}

///////////////////////////////////////////////////////////////////////
// Compute the Phong local illumination color.
//
// Parameters:
// V: the direction towards the eye
// N: the "up" direction of the surface
// L: the direction towards the light
// mat: the surface material
// Clight: the light's color
//
// YOU MUST IMPLEMENT THIS FUNCTION
///////////////////////////////////////////////////////////////////////

glm::vec3 local_illumination(const glm::vec3& V, const glm::vec3& N,
                             const glm::vec3& L,
                             const Material& mat, const glm::vec3& Clight) {


    glm::vec3 R = mirror_direction(L, N);
    glm::vec3 Kd = mat.get_diffuse();
    glm::vec3 Ks = mat.get_specular();
    float n = mat.get_shininess();

    return Clight * (Kd * glm::dot(N, L) + Ks * glm::pow(glm::dot(R, V), n));
}

//////////////////////////////////////////////////////
// This function sets up a simple scene.
//
// THIS FUNCTION IS INCOMPLETE.
// FINISH THE IMPLEMENTATION.
/////////////////////////////////////////////////////
void read_scene(const char *filename) {
    int num_materials;
    int num_lights;
    int num_objects;

    Tokenizer toker(filename);

    while (!toker.eof()) {
        string keyword = toker.next_string();
        // cout << "keyword:" << keyword << "\n";

        if (keyword == string("")) {
            continue; // skip blank lines
        }
        else if (keyword == string("#materials")) {
            num_materials = toker.next_number();
            materials.reserve(num_materials);
        }
        else if (keyword == string("#lights")) {
            num_lights = toker.next_number();
            scene_lights.reserve(num_lights);
        }
        else if (keyword == string("#objects")) {
            num_objects = toker.next_number();
            scene_objects.reserve(num_objects);
        }
        else if (keyword.rfind("camera_", 0) == 0) {
            if (keyword == "camera_eye") {
                eye.x = toker.next_number();
                eye.y = toker.next_number();
                eye.z = toker.next_number();
            }
            else if (keyword == "camera_lookat") {
                lookat.x = toker.next_number();
                lookat.y = toker.next_number();
                lookat.z = toker.next_number();
            }
            else if (keyword == "camera_vup") {
                vup.x = toker.next_number();
                vup.y = toker.next_number();
                vup.z = toker.next_number();
            }
            else if (keyword == "camera_clip") {
                clipL = toker.next_number();
                clipR = toker.next_number();
                clipB = toker.next_number();
                clipT = toker.next_number();
                clipN = toker.next_number();
            }
            else if (keyword == "camera_ambient_fraction") {
                ambient_fraction = toker.next_number();
            }
            else {
                cerr << "Unknow keyword: " << keyword << endl;
                exit(EXIT_FAILURE);
            }
        }
        else if (keyword == "material") {
            glm::vec3 ambient, diffuse, specular;
            float shininess;

            toker.match("ambient");
            ambient.x = toker.next_number();
            ambient.y = toker.next_number();
            ambient.z = toker.next_number();

            toker.match("diffuse");
            diffuse.x = toker.next_number();
            diffuse.y = toker.next_number();
            diffuse.z = toker.next_number();

            toker.match("specular");
            specular.x = toker.next_number();
            specular.y = toker.next_number();
            specular.z = toker.next_number();

            toker.match("shininess");
            shininess = toker.next_number();

            materials.push_back(Material(ambient, diffuse, specular, shininess));
        }
        else if (keyword == "light") {
            
            glm::vec3 color, position;

            toker.match("color");
            color.x = toker.next_number();
            color.y = toker.next_number();
            color.z = toker.next_number();

            toker.match("position");
            position.x = toker.next_number();
            position.y = toker.next_number();
            position.z = toker.next_number();

            scene_lights.push_back(Light(color, position));
        }
        else if (keyword == "sphere") {
            glm::vec3 center;
            float radius;
            int material;

            toker.match("center");
            center.x = toker.next_number();
            center.y = toker.next_number();
            center.z = toker.next_number();

            toker.match("radius");
            radius = toker.next_number();

            toker.match("material");
            material = toker.next_number();

            Object *obj = new Sphere(center, radius, materials[material]);
            scene_objects.push_back(obj);
        }
        else if (keyword == string("triangle")) {
            glm::vec3 A, B, C;
            int materialID;

            toker.match("vertex");
            A.x = toker.next_number();
            A.y = toker.next_number();
            A.z = toker.next_number();

            toker.match("vertex");
            B.x = toker.next_number();
            B.y = toker.next_number();
            B.z = toker.next_number();

            toker.match("vertex");
            C.x = toker.next_number();
            C.y = toker.next_number();
            C.z = toker.next_number();

            toker.match("material");
            materialID = toker.next_number();

            scene_objects.push_back(new Triangle(A, B, C,
                                                 materials[materialID]));
        }

        else {
            cerr << "Parse error: unrecognized keyword \""
                 << keyword << "\"\n";
            exit(EXIT_FAILURE);
        }
    }    
}

// IMPLEMENT THE FUNCTIONS ABOVE THIS LINE
//====================================================================

//////////////////////////////////////////////////////////////////////
// If window size has changed, re-allocate the frame buffer
// You don't have to change this function.
//////////////////////////////////////////////////////////////////////

void check_for_resize() {
    // Now, check if the frame buffer needs to be created,
    // or re-created.

    bool should_allocate = false;
    if (img == NULL) {
        // frame buffer not yet allocated.
        should_allocate = true;
    }
    else if (winWidth  != cam.get_win_W() ||
             winHeight != cam.get_win_H()) {

        // frame buffer allocated, but has changed size.
        delete[] img;
        should_allocate = true;
        winWidth  = cam.get_win_W();
        winHeight = cam.get_win_H();
    }

    if (should_allocate) {

        // cout << "ALLOCATING: (W H)=(" << winWidth
        //      << " " << winHeight << ")\n";

        img = new GLubyte[winWidth * winHeight * 3];
        camera_changed();
    }
}

/////////////////////////////////////////////////////////
// This function actually generates the ray-traced image.
// You don't have to change this function.
/////////////////////////////////////////////////////////

void render() {
    int x,y;
    GLubyte r,g,b;
    int p;

    ambient_light = glm::vec3(0,0,0);
    for (auto light : scene_lights) {
        ambient_light += light.get_color() * ambient_fraction;
    }

    for (y=0; y<winHeight; y++) {
        for (x=0; x<winWidth; x++) {

            //debugOn = (y == winHeight / 2 && x == winHeight / 2);

            if (debugOn) {
                cout << "pixel (" << x << " " << y << ")\n";
                cout.flush();
            }

            glm::vec3 pixel_color = ray_color(x, y);
            pixel_color = glm::clamp(pixel_color, 0.0f, 1.0f);

            r = (GLubyte) (pixel_color.r * 255.0);
            g = (GLubyte) (pixel_color.g * 255.0);
            b = (GLubyte) (pixel_color.b * 255.0);

            p = (y*winWidth + x) * 3;

            img[p]   = r;
            img[p+1] = g;
            img[p+2] = b;
        }
    }
}

//////////////////////////////////////////////////////
//
// Displays, on STDOUT, the colour of the pixel that
//  the user clicked on.
//
// THIS IS VERY USEFUL!  USE IT!
//
// You don't have to change this function.
//
//////////////////////////////////////////////////////

void mouse_button_callback( GLFWwindow* window, int button,
                            int action, int mods )
{
    if (button != GLFW_MOUSE_BUTTON_LEFT)
        return;

    if (action == GLFW_PRESS)
    {
        debugOn = true;

        // Get the mouse's position.

        double xpos, ypos;
        int W, H;
        glfwGetCursorPos(window, &xpos, &ypos);
        glfwGetWindowSize(window, &W, &H);

        // mouse position, as a fraction of the window dimensions
        // The y mouse coord increases as you move down,
        // but our yDCS increases as you move up.
        double mouse_fx = xpos / float(W);
        double mouse_fy = (W - 1 - ypos) / float(W);

        int xDCS = (int)(mouse_fx * winWidth + 0.5);
        int yDCS = (int)(mouse_fy * winHeight + 0.5);

        glm::vec3 pixelColor = ray_color(xDCS, yDCS);

        if (debugOn) {
            cout << "cursorpos:" << xpos << " " << ypos << "\n";
            cout << "Width Height: " << winWidth << " " << winHeight << "\n";
            cout << "Window Size: " << W << " " << H << "\n";
            cout << "Pixel at (x y)=(" << xDCS << " " << yDCS << ")\n";
            cout << "Pixel Color = " << glm::to_string(pixelColor) << endl;
        }
        debugOn = false;
    }
}

/////////////////////////////////////////////////////////
// Call this when scene has changed, and we need to re-run
// the ray tracer.
// You don't have to change this function.
/////////////////////////////////////////////////////////

void camera_changed() {
    float dummy=0;
    cam_param_changed(dummy);
}

/////////////////////////////////////////////////////////
// Called when user modifies a camera parameter.
// You don't have to change this function.
/////////////////////////////////////////////////////////
void cam_param_changed(float param) {
    setup_camera();
    frame_buffer_stale = true;
}

/////////////////////////////////////////////////////////
// Check if window was resized.
// You don't have to change this function.
/////////////////////////////////////////////////////////

bool get_was_window_resized() {
    int new_W = cam.get_win_W();
    int new_H = cam.get_win_H();

    // cout << "window resized to " << new_W << " " << new_H << "\n";

    if (new_W != winWidth || new_H != winHeight) {
        camera_changed();
        winWidth = new_W;
        winHeight = new_H;
        return true;
    }

    return false;
}

///////////////////////////////////////////////////
// Resets the camera parameters.
// You don't have to change this function.
///////////////////////////////////////////////////

void reset_camera(float dummy) {
    eye = eye_home;
    lookat = lookat_home;
    vup = vup_home;
    
    clipL = clip_home[0];
    clipR = clip_home[1];
    clipB = clip_home[2];
    clipT = clip_home[3];
    clipN = clip_home[4];

    ambient_fraction = ambient_fraction_home;

    camera_changed();
}


/////////////////////////////////////////////////////////
// Called on a GLFW error.
// You don't have to change this function.
/////////////////////////////////////////////////////////

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

//////////////////////////////////////////////////////
// Quit if the user hits "q" or "ESC".
// All other key presses are passed to the UI.
// You don't have to change this function.
//////////////////////////////////////////////////////

static void key_callback(GLFWwindow* window, int key,
                         int scancode, int action, int mods)
{
    if (key == GLFW_KEY_Q ||
        key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    else if (action == GLFW_RELEASE) {
        the_ui.handle_key(key);
    }
}


//////////////////////////////////////////////////////
// Show the image.
// You don't have to change this function.
//////////////////////////////////////////////////////

void display () {
    glClearColor(.1f,.1f,.1f, 1.f);   /* set the background colour */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    cam.begin_drawing();

    glRasterPos3d(0.0, 0.0, 0.0);

    glPixelStorei(GL_PACK_ALIGNMENT,1);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);

    if (frame_buffer_stale) {
        //
        // Don't re-render the scene EVERY time display() is called.
        // It might get called if the window is moved, or if it
        // is exposed.  Only re-render if the window is RESIZED.
        // Resizing triggers a call to handleReshape, which sets
        // frameBufferStale.
        //
        render();
        frame_buffer_stale = false;
    }

    //
    // This paints the current image buffer onto the screen.
    //
    glDrawPixels(winWidth,winHeight,
                 GL_RGB,GL_UNSIGNED_BYTE,img);

    glFlush();
}

///////////////////////////////////////////////////////////////////
// Set up the keyboard UI.
// No need to change this.
//////////////////////////////////////////////////////////////////
void init_UI() {
    // These variables will trigger a call-back when they are changed.
    the_ui.add_variable("Eye X", &eye.x, -10, 10, 0.2, cam_param_changed);
    the_ui.add_variable("Eye Y", &eye.y, -10, 10, 0.2, cam_param_changed);
    the_ui.add_variable("Eye Z", &eye.z, -10, 10, 0.2, cam_param_changed);

    the_ui.add_variable("Shadows", &show_shadows, 0, 1, 1, cam_param_changed);
    the_ui.add_variable("Ambient Fraction", &ambient_fraction, 0, 1, 0.1,
        cam_param_changed);

    the_ui.add_variable("Ref X", &lookat.x, -10, 10, 0.2, cam_param_changed);
    the_ui.add_variable("Ref Y", &lookat.y, -10, 10, 0.2, cam_param_changed);
    the_ui.add_variable("Ref Z", &lookat.z, -10, 10, 0.2, cam_param_changed);

    the_ui.add_variable("Vup X", &vup.x, -10, 10, 0.2, cam_param_changed);
    the_ui.add_variable("Vup Y", &vup.y, -10, 10, 0.2, cam_param_changed);
    the_ui.add_variable("Vup Z", &vup.z, -10, 10, 0.2, cam_param_changed);

    the_ui.add_variable("Clip L", &clipL,   -10, 10, 0.2, cam_param_changed);
    the_ui.add_variable("Clip R", &clipR,   -10, 10, 0.2, cam_param_changed);
    the_ui.add_variable("Clip B", &clipB,   -10, 10, 0.2, cam_param_changed);
    the_ui.add_variable("Clip T", &clipT,   -10, 10, 0.2, cam_param_changed);
    the_ui.add_variable("Clip N", &clipN,   -10, 10, 0.2, cam_param_changed);

    static float dummy2=0;
    the_ui.add_variable("Reset Camera", &dummy2, 0, winWidth,
                        0.001, reset_camera);

    the_ui.done_init();

}

//////////////////////////////////////////////////////
// Main program.
// You don't have to change this function.
//////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    init_UI();
    if (argc < 2) {
        cerr << "Usage:\n";
        cerr << "  rt <scene-file.txt>\n";
        cerr << "press Control-C to exit\n";
        char line[100];
        cin >> line;
        exit(EXIT_FAILURE);
    }

    read_scene(argv[1]);

    GLFWwindow* window;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        cerr << "glfwInit failed!\n";
        cerr << "PRESS Control-C to quit\n";
        char line[100];
        cin >> line;
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    window = glfwCreateWindow(winWidth, winHeight,
                              "Ray Traced Scene", NULL, NULL);

    if (!window)
    {
        cerr << "glfwCreateWindow failed!\n";
        cerr << "PRESS Control-C to quit\n";
        char line[100];
        cin >> line;

        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    int w = winWidth;
    int h = winHeight;

    cam = Camera(0,0, w,h, w, h, window);
    setup_camera();

    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glfwSwapInterval(1);

    while (!glfwWindowShouldClose(window))
    {
        cam.check_resize();
        setup_camera();

        display();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    glfwTerminate();
    exit(EXIT_SUCCESS);
}



