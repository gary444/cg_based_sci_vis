#include "application_solar.hpp"
#include "window_handler.hpp"

#include "utils.hpp"
#include "shader_loader.hpp"
#include "model_loader.hpp"




#include <glbinding/gl/gl.h>
// use gl definitions from glbinding 
using namespace gl;



// extra headers for SciVis framework
#include "transfer_function.hpp"
#include "volume_loader_raw.hpp"
#include "cube.hpp"

// #include <imgui.h>
// // #include <imgui_impl_glfw_gl3.h>
// #include <imgui_impl_opengl2.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"


//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <algorithm>
#include <sstream>

#define IM_ARRAYSIZE(_ARR)          ((int)(sizeof(_ARR)/sizeof(*_ARR)))


std::string g_file_string = "../../../data/head_w256_h256_d225_c1_b8.raw";

// set the sampling distance for the ray traversal
float       g_sampling_distance = 0.001f;
float       g_sampling_distance_fact = 0.5f;
float       g_sampling_distance_fact_move = 2.0f;
// float       g_sampling_distance_fact_ref = 1.0f;

float       g_iso_value = 0.2f;
float       g_iso_value_2 = 0.6f;

// set the light position and color for shading
// set the light position and color for shading
glm::vec3   g_light_pos = glm::vec3(10.0, 10.0, 10.0);
glm::vec3   g_ambient_light_color = glm::vec3(0.3f, 0.3f, 0.3f);
glm::vec3   g_diffuse_light_color = glm::vec3(0.5f, 0.5f, 0.5f);
glm::vec3   g_specula_light_color = glm::vec3(1.f, 1.f, 1.f);
float       g_ref_coef = 12.0;

// set backgorund color here
//glm::vec3   g_background_color = glm::vec3(1.0f, 1.0f, 1.0f); //white
glm::vec3   g_background_color = glm::vec3(0.08f, 0.08f, 0.08f);   //grey

glm::ivec2  g_window_res = glm::ivec2(1600, 800);

// Volume Rendering GLSL Program
GLuint g_volume_program(0);
std::string g_error_message;
bool g_reload_shader_error = false;

Transfer_function g_transfer_fun;
int g_current_tf_data_value = 0;
GLuint g_transfer_texture;
bool g_transfer_dirty = true;
bool g_redraw_tf = true;
bool g_lighting_toggle = false;
// bool g_shadow_toggle = false;
// bool g_opacity_correction_toggle = false;

// imgui variables
static bool g_show_gui = true;
static bool mousePressed[2] = { false, false };

bool g_show_transfer_function_in_window = false;
glm::vec2 g_transfer_function_pos = glm::vec2(0.0f);
glm::vec2 g_transfer_function_size = glm::vec2(0.0f);

//imgui values
bool g_over_gui = false;
bool g_reload_shader = false;
bool g_reload_shader_pressed = false;
bool g_show_transfer_function = false;

int g_task_chosen = 0;
int g_task_chosen_old = g_task_chosen;

bool  g_pause = false;

Volume_loader_raw g_volume_loader;
volume_data_type g_volume_data;
glm::ivec3 g_vol_dimensions;
glm::vec3 g_max_volume_bounds;
unsigned g_channel_size = 0;
unsigned g_channel_count = 0;
GLuint g_volume_texture = 0;

int g_bilinear_interpolation = true;

bool first_frame = true;

bool read_volume(std::string& volume_string){

    //init volume g_volume_loader
    //Volume_loader_raw g_volume_loader;
    //read volume dimensions
    g_vol_dimensions = g_volume_loader.get_dimensions(g_file_string);

    g_sampling_distance = 1.0f / glm::max(glm::max(g_vol_dimensions.x, g_vol_dimensions.y), g_vol_dimensions.z);

    unsigned max_dim = std::max(std::max(g_vol_dimensions.x,
        g_vol_dimensions.y),
        g_vol_dimensions.z);

    // calculating max volume bounds of volume (0.0 .. 1.0)
    g_max_volume_bounds = glm::vec3(g_vol_dimensions) / glm::vec3((float)max_dim);

    // loading volume file data
    g_volume_data = g_volume_loader.load_volume(g_file_string);
    g_channel_size = g_volume_loader.get_bit_per_channel(g_file_string) / 8;
    g_channel_count = g_volume_loader.get_channel_count(g_file_string);

    // setting up proxy geometry
        Cube g_cube;

    g_cube.freeVAO();
    g_cube = Cube(glm::vec3(0.0, 0.0, 0.0), g_max_volume_bounds);

    glActiveTexture(GL_TEXTURE0);
    g_volume_texture = createTexture3D(g_vol_dimensions.x, g_vol_dimensions.y, g_vol_dimensions.z, g_channel_size, g_channel_count, (char*)&g_volume_data[0]);

    return 0 != g_volume_texture;

}


void UpdateImGui()
{
    ImGuiIO& io = ImGui::GetIO();

    // // Setup resolution (every frame to accommodate for window resizing)    
    // int w = 1920;
    // int h = 1080;
    // int display_w = w;
    // int display_h = h;
    int w, h;
    int display_w, display_h;
    glfwGetWindowSize(window, &w, &h);
    glfwGetFramebufferSize(window, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)display_w, (float)display_h);                                   // Display size, in pixels. For clamping windows positions.

    // Setup time step
    static double time = 0.0f;
    const double current_time = glfwGetTime();
    io.DeltaTime = (float)(current_time - time);
    time = current_time;
    
    // Setup inputs
    // (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
    double mouse_x, mouse_y;
    glfwGetCursorPos(window, &mouse_x, &mouse_y);
    mouse_x *= (float)display_w / w;                                                               // Convert mouse coordinates to pixels
    mouse_y *= (float)display_h / h;
    io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);                                          // Mouse position, in pixels (set to -1,-1 if no mouse / on another screen, etc.)
    io.MouseDown[0] = mousePressed[0] || glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != 0;  // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
    io.MouseDown[1] = mousePressed[1] || glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != 0;

    // Start the frame
    // ImGui::NewFrame();
    ImGui_ImplOpenGL2_NewFrame();
}

void showGUI(){

    ImGui::SetNextWindowPos(ImVec2(50, 50),ImGuiSetCond_FirstUseEver);

    ImGui::Begin("Volume Settings", &g_show_gui, ImVec2(300, 500));
    static float f;
    g_over_gui = ImGui::IsMouseHoveringAnyWindow();

    // Calculate and show frame rate
    static ImVector<float> ms_per_frame; if (ms_per_frame.empty()) { ms_per_frame.resize(400); memset(&ms_per_frame.front(), 0, ms_per_frame.size()*sizeof(float)); }
    static int ms_per_frame_idx = 0;
    static float ms_per_frame_accum = 0.0f;
    if (!g_pause){
        ms_per_frame_accum -= ms_per_frame[ms_per_frame_idx];
        ms_per_frame[ms_per_frame_idx] = ImGui::GetIO().DeltaTime * 1000.0f;
        ms_per_frame_accum += ms_per_frame[ms_per_frame_idx];

        ms_per_frame_idx = (ms_per_frame_idx + 1) % ms_per_frame.size();
    }
    const float ms_per_frame_avg = ms_per_frame_accum / 120;

    if (ImGui::CollapsingHeader("Task", 0, true, true))
    {        
        if (ImGui::TreeNode("Example")){    
            ImGui::RadioButton("Threshold", &g_task_chosen, 0);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Simple")){    
            ImGui::RadioButton("X-Ray", &g_task_chosen, 11);
            ImGui::RadioButton("Angiogram", &g_task_chosen, 12);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Iso-Surface Rendering")){
            ImGui::RadioButton("First-Hit", &g_task_chosen, 13);
            ImGui::RadioButton("Multi-Iso-Surface Compositing", &g_task_chosen, 24);
            ImGui::SliderFloat("Iso Value 1", &g_iso_value, 0.0f, 1.0f, "%.8f", 1.0f);
            ImGui::SliderFloat("Iso Value 2", &g_iso_value_2, 0.0f, 1.0f, "%.8f", 1.0f);
            ImGui::TreePop();
        }       
 
        if (ImGui::TreeNode("Direct Volume Rendering")){
            ImGui::RadioButton("Front-to-Back Compositing", &g_task_chosen, 21);
            ImGui::RadioButton("Adaptive Sampling", &g_task_chosen, 25);
            ImGui::TreePop();
        }

        
        g_reload_shader ^= ImGui::Checkbox("1", &g_lighting_toggle); ImGui::SameLine();
        g_task_chosen == 0 || g_task_chosen == 12 || g_task_chosen == 12 ? ImGui::TextColored(ImVec4(0.2f, 0.2f, 0.2f, 0.5f), "Enable Lighting") : ImGui::Text("Enable Lighting");

        // g_reload_shader ^= ImGui::Checkbox("2", &g_shadow_toggle); ImGui::SameLine();
        // g_task_chosen == 11 || g_task_chosen == 14 ? ImGui::Text("Enable Shadows") : ImGui::TextColored(ImVec4(0.2f, 0.2f, 0.2f, 0.5f), "Enable Shadows");

        // g_reload_shader ^= ImGui::Checkbox("3", &g_opacity_correction_toggle); ImGui::SameLine();
        // g_task_chosen == 21 ? ImGui::Text("Opacity Correction") : ImGui::TextColored(ImVec4(0.2f, 0.2f, 0.2f, 0.5f), "Opacity Correction");

        if (g_task_chosen != g_task_chosen_old){
            g_reload_shader = true;
            g_task_chosen_old = g_task_chosen;
        }
    }

    if (ImGui::CollapsingHeader("Load Volumes", 0, true, false))
    {


        bool load_volume_1 = false;
        bool load_volume_2 = false;
        bool load_volume_3 = false;

        ImGui::Text("Volumes");
        load_volume_1 ^= ImGui::Button("Load Volume Head");
        load_volume_2 ^= ImGui::Button("Load Volume Engine");
        load_volume_3 ^= ImGui::Button("Load Volume Bucky");


        if (load_volume_1){
            g_file_string = "../../../data/head_w256_h256_d225_c1_b8.raw";
            read_volume(g_file_string);
        }
        if (load_volume_2){
            g_file_string = "../../../data/Engine_w256_h256_d256_c1_b8.raw";
            read_volume(g_file_string);
        }

        if (load_volume_3){
            g_file_string = "../../../data/Bucky_uncertainty_data_w32_h32_d32_c1_b8.raw";
            read_volume(g_file_string);
        }
    }


    if (ImGui::CollapsingHeader("Lighting Settings"))
    {
        ImGui::SliderFloat3("Position Light", &g_light_pos[0], -10.0f, 10.0f);

        ImGui::ColorEdit3("Ambient Color", &g_ambient_light_color[0]);
        ImGui::ColorEdit3("Diffuse Color", &g_diffuse_light_color[0]);
        ImGui::ColorEdit3("Specular Color", &g_specula_light_color[0]);

        ImGui::SliderFloat("Reflection Coefficient kd", &g_ref_coef, 0.0f, 20.0f, "%.5f", 1.0f);


    }

    if (ImGui::CollapsingHeader("Quality Settings"))
    {
        ImGui::Text("Interpolation");
        ImGui::RadioButton("Nearest Neighbour", &g_bilinear_interpolation, 0);
        ImGui::RadioButton("Bilinear", &g_bilinear_interpolation, 1);

        ImGui::Text("Sampling Size");
        ImGui::SliderFloat("sampling factor", &g_sampling_distance_fact, 0.1f, 10.0f, "%.5f", 4.0f);
        ImGui::SliderFloat("sampling factor move", &g_sampling_distance_fact_move, 0.1f, 10.0f, "%.5f", 4.0f);
        // ImGui::SliderFloat("reference sampling factor", &g_sampling_distance_fact_ref, 0.1f, 10.0f, "%.5f", 4.0f);
    }

    if (ImGui::CollapsingHeader("Shader", 0, true, true))
    {
        static ImVec4 text_color(1.0, 1.0, 1.0, 1.0);

        if (g_reload_shader_error) {
            text_color = ImVec4(1.0, 0.0, 0.0, 1.0);
            ImGui::TextColored(text_color, "Shader Error");
        }
        else
        {
            text_color = ImVec4(0.0, 1.0, 0.0, 1.0);
            ImGui::TextColored(text_color, "Shader Ok");
        }

        
        ImGui::TextWrapped(g_error_message.c_str());

        g_reload_shader ^= ImGui::Button("Reload Shader");

    }

    if (ImGui::CollapsingHeader("Timing"))
    {
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", ms_per_frame_avg, 1000.0f / ms_per_frame_avg);

        float min = *std::min_element(ms_per_frame.begin(), ms_per_frame.end());
        float max = *std::max_element(ms_per_frame.begin(), ms_per_frame.end());

        if (max - min < 10.0f){
            float mid = (max + min) * 0.5f;
            min = mid - 5.0f;
            max = mid + 5.0f;
        }

        static size_t values_offset = 0;

        char buf[50];
        sprintf(buf, "avg %f", ms_per_frame_avg);
        ImGui::PlotLines("Frame Times", &ms_per_frame.front(), (int)ms_per_frame.size(), (int)values_offset, buf, min - max * 0.1f, max * 1.1f, ImVec2(0, 70));

        ImGui::SameLine(); ImGui::Checkbox("pause", &g_pause);

    }

    if (ImGui::CollapsingHeader("Window options"))
    {        
        // if (ImGui::TreeNode("Window Size")){
        //     const char* items[] = { "640x480", "720x576", "1280x720", "1920x1080", "1920x1200", "2048x1536" };
        //     static int item2 = -1;
        //     bool press = ImGui::Combo("Window Size", &item2, items, IM_ARRAYSIZE(items));    

        //     if (press){
        //         glm::ivec2 win_re_size = glm::ivec2(640, 480);

        //         switch (item2){
        //         case 0:
        //             win_re_size = glm::ivec2(640, 480);
        //             break;
        //         case 1:
        //             win_re_size = glm::ivec2(720, 576);
        //             break;
        //         case 2:
        //             win_re_size = glm::ivec2(1280, 720);
        //             break;
        //         case 3:
        //             win_re_size = glm::ivec2(1920, 1080);
        //             break;
        //         case 4:
        //             win_re_size = glm::ivec2(1920, 1200);
        //             break;
        //         case 5:
        //             win_re_size = glm::ivec2(1920, 1536);
        //             break;
        //         default:
        //             break;
        //         }                
        //         // g_win.resize(win_re_size);                
        //     }

        //     ImGui::TreePop();
        // }
        
        if (ImGui::TreeNode("Background Color")){
            ImGui::ColorEdit3("BC", &g_background_color[0]);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Style Editor"))
        {
            ImGui::ShowStyleEditor();
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Logging"))
        {
            ImGui::LogButtons();
            ImGui::TreePop();
        }
    }

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(450, 50),ImGuiSetCond_FirstUseEver);
    
    g_show_transfer_function = ImGui::Begin("Transfer Function Window", &g_show_transfer_function_in_window, ImVec2(300, 500));

    g_transfer_function_pos.x = ImGui::GetItemBoxMin().x;
    g_transfer_function_pos.y = ImGui::GetItemBoxMin().y;

    g_transfer_function_size.x = ImGui::GetItemBoxMax().x - ImGui::GetItemBoxMin().x;
    g_transfer_function_size.y = ImGui::GetItemBoxMax().y - ImGui::GetItemBoxMin().y;

    static unsigned byte_size = 255;

    static ImVector<float> A; if (A.empty()){ A.resize(byte_size); }

    if (g_redraw_tf){
        g_redraw_tf = false;

        image_data_type color_con = g_transfer_fun.get_RGBA_transfer_function_buffer();

        for (unsigned i = 0; i != byte_size; ++i){
            A[i] = color_con[i * 4 + 3];
        }
    }

    ImGui::PlotLines("", &A.front(), (int)A.size(), (int)0, "", 0.0, 255.0, ImVec2(0, 70));

    g_transfer_function_pos.x = ImGui::GetItemBoxMin().x;
    g_transfer_function_pos.y = ImGui::GetIO().DisplaySize.y - ImGui::GetItemBoxMin().y - 70;

    g_transfer_function_size.x = ImGui::GetItemBoxMax().x - ImGui::GetItemBoxMin().x;
    g_transfer_function_size.y = ImGui::GetItemBoxMax().y - ImGui::GetItemBoxMin().y;

    ImGui::SameLine(); ImGui::Text("Color:RGB Plot: Alpha");

    static int data_value = 0;
    ImGui::SliderInt("Data Value", &data_value, 0, 255);
    static float col[4] = { 0.4f, 0.7f, 0.0f, 0.5f };
    ImGui::ColorEdit4("color", col);
    bool add_entry_to_tf = false;
    add_entry_to_tf ^= ImGui::Button("Add entry"); ImGui::SameLine();

    bool reset_tf = false;
    reset_tf ^= ImGui::Button("Reset");

    if (reset_tf){
        g_transfer_fun.reset();
        g_transfer_dirty = true;
        g_redraw_tf = true;
    }

    if (add_entry_to_tf){
        g_current_tf_data_value = data_value;
        g_transfer_fun.add((unsigned)data_value, glm::vec4(col[0], col[1], col[2], col[3]));
        g_transfer_dirty = true;
        g_redraw_tf = true;
    }

    if (ImGui::CollapsingHeader("Manipulate Values")){


        Transfer_function::container_type con = g_transfer_fun.get_piecewise_container();

        bool delete_entry_from_tf = false;

        static std::vector<int> g_c_data_value;

        if (g_c_data_value.size() != con.size())
            g_c_data_value.resize(con.size());

        int i = 0;

        for (Transfer_function::container_type::iterator c = con.begin(); c != con.end(); ++c)
        {

            int c_data_value = c->first;
            glm::vec4 c_color_value = c->second;

            g_c_data_value[i] = c_data_value;

            std::stringstream ss;
            if (c->first < 10)
                ss << c->first << "  ";
            else if (c->first < 100)
                ss << c->first << " ";
            else
                ss << c->first;

            bool change_value = false;
            change_value ^= ImGui::SliderInt(std::to_string(i).c_str(), &g_c_data_value[i], 0, 255); ImGui::SameLine();

            if (change_value){
                if (con.find(g_c_data_value[i]) == con.end()){
                    g_transfer_fun.remove(c_data_value);
                    g_transfer_fun.add((unsigned)g_c_data_value[i], c_color_value);
                    g_current_tf_data_value = g_c_data_value[i];
                    g_transfer_dirty = true;
                    g_redraw_tf = true;
                }
            }

            //delete             
            bool delete_entry_from_tf = false;
            delete_entry_from_tf ^= ImGui::Button(std::string("Delete: ").append(ss.str()).c_str());

            if (delete_entry_from_tf){
                g_current_tf_data_value = c_data_value;
                g_transfer_fun.remove(g_current_tf_data_value);
                g_transfer_dirty = true;
                g_redraw_tf = true;
            }

            static float n_col[4] = { 0.4f, 0.7f, 0.0f, 0.5f };
            memcpy(&n_col, &c_color_value, sizeof(float)* 4);

            bool change_color = false;
            change_color ^= ImGui::ColorEdit4(ss.str().c_str(), n_col);

            if (change_color){
                g_transfer_fun.add((unsigned)g_c_data_value[i], glm::vec4(n_col[0], n_col[1], n_col[2], n_col[3]));
                g_current_tf_data_value = g_c_data_value[i];
                g_transfer_dirty = true;
                g_redraw_tf = true;
            }

            ImGui::Separator();

            ++i;
        }
    }


    if (ImGui::CollapsingHeader("Transfer Function - Save/Load", 0, true, false))
    {

        ImGui::Text("Transferfunctions");
        bool load_tf_1 = false;
        bool load_tf_2 = false;
        bool load_tf_3 = false;
        bool load_tf_4 = false;
        bool load_tf_5 = false;
        bool load_tf_6 = false;
        bool save_tf_1 = false;
        bool save_tf_2 = false;
        bool save_tf_3 = false;
        bool save_tf_4 = false;
        bool save_tf_5 = false;
        bool save_tf_6 = false;

        save_tf_1 ^= ImGui::Button("Save TF1"); ImGui::SameLine();
        load_tf_1 ^= ImGui::Button("Load TF1");
        save_tf_2 ^= ImGui::Button("Save TF2"); ImGui::SameLine();
        load_tf_2 ^= ImGui::Button("Load TF2");
        save_tf_3 ^= ImGui::Button("Save TF3"); ImGui::SameLine();
        load_tf_3 ^= ImGui::Button("Load TF3");
        save_tf_4 ^= ImGui::Button("Save TF4"); ImGui::SameLine();
        load_tf_4 ^= ImGui::Button("Load TF4");
        save_tf_5 ^= ImGui::Button("Save TF5"); ImGui::SameLine();
        load_tf_5 ^= ImGui::Button("Load TF5");
        save_tf_6 ^= ImGui::Button("Save TF6"); ImGui::SameLine();
        load_tf_6 ^= ImGui::Button("Load TF6");

        if (save_tf_1 || save_tf_2 || save_tf_3 || save_tf_4 || save_tf_5 || save_tf_6){
            Transfer_function::container_type con = g_transfer_fun.get_piecewise_container();
            std::vector<Transfer_function::element_type> save_vect;

            for (Transfer_function::container_type::iterator c = con.begin(); c != con.end(); ++c)
            {
                save_vect.push_back(*c);
            }

            std::ofstream tf_file;

            if (save_tf_1){ tf_file.open("TF1.tf", std::ios::out | std::ofstream::binary); }
            if (save_tf_2){ tf_file.open("TF2.tf", std::ios::out | std::ofstream::binary); }
            if (save_tf_3){ tf_file.open("TF3.tf", std::ios::out | std::ofstream::binary); }
            if (save_tf_4){ tf_file.open("TF4.tf", std::ios::out | std::ofstream::binary); }
            if (save_tf_5){ tf_file.open("TF5.tf", std::ios::out | std::ofstream::binary); }
            if (save_tf_6){ tf_file.open("TF6.tf", std::ios::out | std::ofstream::binary); }

            //std::copy(save_vect.begin(), save_vect.end(), std::ostreambuf_iterator<char>(tf_file));
            tf_file.write((char*)&save_vect[0], sizeof(Transfer_function::element_type) * save_vect.size());
            tf_file.close();
        }

        if (load_tf_1 || load_tf_2 || load_tf_3 || load_tf_4 || load_tf_5 || load_tf_6){
            Transfer_function::container_type con = g_transfer_fun.get_piecewise_container();
            std::vector<Transfer_function::element_type> load_vect;

            std::ifstream tf_file;

            if (load_tf_1){ tf_file.open("TF1.tf", std::ios::in | std::ifstream::binary); }
            if (load_tf_2){ tf_file.open("TF2.tf", std::ios::in | std::ifstream::binary); }
            if (load_tf_3){ tf_file.open("TF3.tf", std::ios::in | std::ifstream::binary); }
            if (load_tf_4){ tf_file.open("TF4.tf", std::ios::in | std::ifstream::binary); }
            if (load_tf_5){ tf_file.open("TF5.tf", std::ios::in | std::ifstream::binary); }
            if (load_tf_6){ tf_file.open("TF6.tf", std::ios::in | std::ifstream::binary); }


            if (tf_file.good()){
                tf_file.seekg(0, tf_file.end);

                size_t size = tf_file.tellg();
                unsigned elements = (int)size / (unsigned)sizeof(Transfer_function::element_type);
                tf_file.seekg(0);

                load_vect.resize(elements);
                tf_file.read((char*)&load_vect[0], size);

                g_transfer_fun.reset();
                g_transfer_dirty = true;
                for (std::vector<Transfer_function::element_type>::iterator c = load_vect.begin(); c != load_vect.end(); ++c)
                {
                    g_transfer_fun.add(c->first, c->second);
                }
            }

            tf_file.close();

        }

    }

    ImGui::End();
}

ApplicationSolar::ApplicationSolar(std::string const& resource_path)
 :Application{resource_path}
 ,planet_object{}
 ,m_view_transform{glm::translate(glm::fmat4{}, glm::fvec3{0.0f, 0.0f, 4.0f})}
 ,m_view_projection{utils::calculate_projection_matrix(initial_aspect_ratio)}
{
  initializeGeometry();
  initializeShaderPrograms();


  g_transfer_fun.init();

  // ImGui_ImplGlfwGL3_Init(g_win.getGLFWwindow(), true);
  ImGui_ImplOpenGL2_Init(window, true);


}

ApplicationSolar::~ApplicationSolar() {
  glDeleteBuffers(1, &planet_object.vertex_BO);
  glDeleteBuffers(1, &planet_object.element_BO);
  glDeleteVertexArrays(1, &planet_object.vertex_AO);
}

void ApplicationSolar::render() const {
  // bind shader to upload uniforms
  glUseProgram(m_shaders.at("planet").handle);

  glm::fmat4 model_matrix = glm::rotate(glm::fmat4{}, float(glfwGetTime()), glm::fvec3{0.0f, 1.0f, 0.0f});
  model_matrix = glm::translate(model_matrix, glm::fvec3{0.0f, 0.0f, -1.0f});
  glUniformMatrix4fv(m_shaders.at("planet").u_locs.at("ModelMatrix"),
                     1, GL_FALSE, glm::value_ptr(model_matrix));

  // extra matrix for normal transformation to keep them orthogonal to surface
  glm::fmat4 normal_matrix = glm::inverseTranspose(glm::inverse(m_view_transform) * model_matrix);
  glUniformMatrix4fv(m_shaders.at("planet").u_locs.at("NormalMatrix"),
                     1, GL_FALSE, glm::value_ptr(normal_matrix));

  // bind the VAO to draw
  glBindVertexArray(planet_object.vertex_AO);

  // draw bound vertex array using bound shader
  glDrawElements(planet_object.draw_mode, planet_object.num_elements, model::INDEX.type, NULL);

    //IMGUI ROUTINE begin    
    ImGuiIO& io = ImGui::GetIO();
    io.MouseWheel = 0;
    mousePressed[0] = mousePressed[1] = false;
    glfwPollEvents();
    UpdateImGui();
    showGUI();
    // Rendering
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui::Render();
}

void ApplicationSolar::uploadView() {
  // vertices are transformed in camera space, so camera transform must be inverted
  glm::fmat4 view_matrix = glm::inverse(m_view_transform);
  // upload matrix to gpu
  glUniformMatrix4fv(m_shaders.at("planet").u_locs.at("ViewMatrix"),
                     1, GL_FALSE, glm::value_ptr(view_matrix));
}

void ApplicationSolar::uploadProjection() {
  // upload matrix to gpu
  glUniformMatrix4fv(m_shaders.at("planet").u_locs.at("ProjectionMatrix"),
                     1, GL_FALSE, glm::value_ptr(m_view_projection));
}

// update uniform locations
void ApplicationSolar::uploadUniforms() { 
  // bind shader to which to upload unforms
  glUseProgram(m_shaders.at("planet").handle);
  // upload uniform values to new locations
  uploadView();
  uploadProjection();
}

///////////////////////////// intialisation functions /////////////////////////
// load shader sources
void ApplicationSolar::initializeShaderPrograms() {
  // store shader program objects in container
  m_shaders.emplace("planet", shader_program{{{GL_VERTEX_SHADER,m_resource_path + "shaders/simple.vert"},
                                           {GL_FRAGMENT_SHADER, m_resource_path + "shaders/simple.frag"}}});
  // request uniform locations for shader program
  m_shaders.at("planet").u_locs["NormalMatrix"] = -1;
  m_shaders.at("planet").u_locs["ModelMatrix"] = -1;
  m_shaders.at("planet").u_locs["ViewMatrix"] = -1;
  m_shaders.at("planet").u_locs["ProjectionMatrix"] = -1;
}

// load models
void ApplicationSolar::initializeGeometry() {
  model planet_model = model_loader::obj(m_resource_path + "models/sphere.obj", model::NORMAL);

  // generate vertex array object
  glGenVertexArrays(1, &planet_object.vertex_AO);
  // bind the array for attaching buffers
  glBindVertexArray(planet_object.vertex_AO);

  // generate generic buffer
  glGenBuffers(1, &planet_object.vertex_BO);
  // bind this as an vertex array buffer containing all attributes
  glBindBuffer(GL_ARRAY_BUFFER, planet_object.vertex_BO);
  // configure currently bound array buffer
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * planet_model.data.size(), planet_model.data.data(), GL_STATIC_DRAW);

  // activate first attribute on gpu
  glEnableVertexAttribArray(0);
  // first attribute is 3 floats with no offset & stride
  glVertexAttribPointer(0, model::POSITION.components, model::POSITION.type, GL_FALSE, planet_model.vertex_bytes, planet_model.offsets[model::POSITION]);
  // activate second attribute on gpu
  glEnableVertexAttribArray(1);
  // second attribute is 3 floats with no offset & stride
  glVertexAttribPointer(1, model::NORMAL.components, model::NORMAL.type, GL_FALSE, planet_model.vertex_bytes, planet_model.offsets[model::NORMAL]);

   // generate generic buffer
  glGenBuffers(1, &planet_object.element_BO);
  // bind this as an vertex array buffer containing all attributes
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planet_object.element_BO);
  // configure currently bound array buffer
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, model::INDEX.size * planet_model.indices.size(), planet_model.indices.data(), GL_STATIC_DRAW);

  // store type of primitive to draw
  planet_object.draw_mode = GL_TRIANGLES;
  // transfer number of indices to model object 
  planet_object.num_elements = GLsizei(planet_model.indices.size());
}

///////////////////////////// callback functions for window events ////////////
// handle key input
void ApplicationSolar::keyCallback(int key, int action, int mods) {
  if (key == GLFW_KEY_W  && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    m_view_transform = glm::translate(m_view_transform, glm::fvec3{0.0f, 0.0f, -0.1f});
    uploadView();
  }
  else if (key == GLFW_KEY_S  && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    m_view_transform = glm::translate(m_view_transform, glm::fvec3{0.0f, 0.0f, 0.1f});
    uploadView();
  }
}

//handle delta mouse movement input
void ApplicationSolar::mouseCallback(double pos_x, double pos_y) {
  // mouse handling
}

//handle resizing
void ApplicationSolar::resizeCallback(unsigned width, unsigned height) {
  // recalculate projection matrix for new aspect ration
  m_view_projection = utils::calculate_projection_matrix(float(width) / float(height));
  // upload new projection matrix
  uploadProjection();
}


// exe entry point
int main(int argc, char* argv[]) {
  Application::run<ApplicationSolar>(argc, argv, 3, 2);
}