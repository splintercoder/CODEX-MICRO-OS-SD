/**
 * @file webos001.cpp
 * @brief Unified ESP32 Base Model XY Screen Driver with Built-In
 * Flat-Shaded Opaque 3D Geon Polyhedral Engine and HTTP Binary Streamer.
 */

#include <string.h>
#include <cmath>
#include <cstdlib> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_task_wdt.h"

#include "codex_geon_engine.h"

#define PIXEL_CLOCK_GPIO GPIO_NUM_4
#define FRAME_WIDTH 320
#define FRAME_HEIGHT 240
#define FRAME_BUFFER_SIZE (FRAME_WIDTH * FRAME_HEIGHT) 
#define TAG "CODEX_BIOS"

///map

// 10x10 Binary Layout: 1 = Ground block, 0 = Water / empty space
static const uint8_t G_BINARY_MAP[10][10] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 1, 1, 1, 1, 0, 0, 1},
    {1, 0, 1, 1, 0, 0, 1, 1, 0, 1},
    {1, 1, 1, 0, 0, 0, 0, 1, 1, 1},
    {1, 1, 0, 0, 1, 1, 0, 0, 1, 1},
    {1, 1, 0, 0, 1, 1, 0, 0, 1, 1},
    {1, 1, 1, 0, 0, 0, 0, 1, 1, 1},
    {1, 0, 1, 1, 0, 0, 1, 1, 0, 1},
    {1, 0, 0, 1, 1, 1, 1, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};

static SceneNode* g_tilemap_head = nullptr;
static bool g_binary_map_loaded = false;

void build_binary_tilemap() {
    uint32_t current_id = 5000; 
    
    // Scale up 10x: individual tiles are now 4.5f wide instead of 0.45f
    const float tile_size = 4.5f; 
    const float map_offset = (10.0f * tile_size) / 2.0f; 

    for (int row = 0; row < 10; row++) {
        for (int col = 0; col < 10; col++) {
            if (G_BINARY_MAP[row][col] == 0) continue;

            SceneNode* map_tile = (SceneNode*)malloc(sizeof(SceneNode));
            if (!map_tile) continue;

            map_tile->id = current_id++;
            map_tile->geon_type = GEON_HEXAHEDRON;
            map_tile->flags = 1; 
            
            // Re-map world translation vectors to the giant scale spacing
            map_tile->x = (col * tile_size) - map_offset + (tile_size / 2.0f);
            map_tile->y = -0.54f; 
            map_tile->z = (row * tile_size) - map_offset + (tile_size / 2.0f);
            
            map_tile->rot_x = 0.0f; map_tile->rot_y = 0.0f; map_tile->rot_z = 0.0f;
            
            // Sizing: 95% of full size keeps a subtle line separation between the huge sectors
            map_tile->scale_x = tile_size * 0.95f; 
            map_tile->scale_y = 0.15f; // Made the floor slabs slightly thicker to look sturdy at scale
            map_tile->scale_z = tile_size * 0.95f;

            map_tile->next = g_tilemap_head;
            g_tilemap_head = map_tile;
        }
    }
    g_binary_map_loaded = true;
}

// Forward declaration of structures 
struct GeonActor {
    float x, y, z; 
    float dx, dy, dz; 
    float rotation; 
    uint8_t base_color; 
};

#define MAX_ACTORS 32
static GeonActor g_world_actors[MAX_ACTORS];
static bool g_actors_initialized = false;

// Global inputs for game control vector mechanics
static float g_input_forward = 0.0f;
static float g_input_turn = 0.0f;

// New Developer & Camera State Variables
static bool g_dev_mode = false;
static float g_camera_zoom = 1.4f;        // Defaults to your original baseline cam_dist
static uint8_t g_selected_geon_type = 0;
/* ========================================================================== */
/* WIFI NETWORK STATION LAYER                                                 */
/* ========================================================================== */
void initialize_wifi_station(void) {
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {};
    strcpy(reinterpret_cast<char*>(wifi_config.sta.ssid), "STARLINK");
    strcpy(reinterpret_cast<char*>(wifi_config.sta.password), "");
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI("WIFI_BOOT", "Connecting to local network channel...");
}

/* ========================================================================== */
/* PARALLEL CLOCK SCANNER CORE TASK                                           */
/* ========================================================================== */
class ClockScanner {
private:
    gpio_num_t scan_pin;
    TaskHandle_t task_handle = nullptr;

    static void thread_worker_thunk(void* param) {
        static_cast<ClockScanner*>(param)->process_loop();
    }

    void process_loop() {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << scan_pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);
        while (true) {
            int level = gpio_get_level(scan_pin);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
public:
    explicit ClockScanner(gpio_num_t target_pin) : scan_pin(target_pin) {}
    bool activate() {
        BaseType_t res = xTaskCreatePinnedToCore(thread_worker_thunk, "clock_scanner_task", 4096, this, 1, &task_handle, 1);
        return (res == pdPASS);
    }
};

/* ========================================================================== */
/* VIDEO SYSTEM CONTAINER WITH OPAQUE LIGHTED 3D ENGINE                       */
/* ========================================================================== */
class VideoSystem {
public:
    uint8_t frame_buffer[FRAME_BUFFER_SIZE];
    uint32_t current_pulse_count = 0;
    float rotation_angle = 0.0f;

    VideoSystem() {
        memset(frame_buffer, 0, FRAME_BUFFER_SIZE);
    }

    void draw_software_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color) {
        if (y0 > y1) { int tx = x0; x0 = x1; x1 = tx; int ty = y0; y0 = y1; y1 = ty; }
        if (y0 > y2) { int tx = x0; x0 = x2; x2 = tx; int ty = y0; y0 = y2; y2 = ty; }
        if (y1 > y2) { int tx = x1; x1 = x2; x2 = tx; int ty = y1; y1 = y2; y2 = ty; }
        if (y0 == y2) return;

        int total_height = y2 - y0;
        for (int y = y0; y <= y2; y++) {
            if (y < 0 || y >= FRAME_HEIGHT) continue;
            bool second_half = y > y1 || y1 == y0;
            int segment_height = second_half ? y2 - y1 : y1 - y0;
            if (segment_height == 0) continue;

            float alpha = (float)(y - y0) / total_height;
            float beta = (float)(y - (second_half ? y1 : y0)) / segment_height;
            int ax = x0 + (int)((x2 - x0) * alpha);
            int bx = second_half ? x1 + (int)((x2 - x1) * beta) : x0 + (int)((x1 - x0) * beta);

            if (ax > bx) { int tx = ax; ax = bx; bx = tx; }
            if (ax < 0) ax = 0;
            if (bx >= FRAME_WIDTH) bx = FRAME_WIDTH - 1;

            uint32_t scanline_row_offset = y * FRAME_WIDTH;
            for (int x = ax; x <= bx; x++) {
                frame_buffer[scanline_row_offset + x] = color;
            }
        }
    }

    void render_geon_software(GeonType type, float tx, float ty, float tz, 
                              float rx, float ry, float rz, 
                              float scale_x, float scale_y, float scale_z, 
                              uint8_t base_color, float cam_rot_y, float cx, float cy, float cz) {
        GeonMeshBuffer mesh = codex_get_geon_mesh(type);
        if (!mesh.vertices || mesh.vertex_count == 0) return;

        int screen_x[64]; int screen_y[64];
        float world_x[64]; float world_y[64]; float world_z[64];
        bool vertex_valid[64];

        float cos_x = std::cos(rx), sin_x = std::sin(rx);
        float cos_y = std::cos(ry), sin_y = std::sin(ry);
        float cos_z = std::cos(rz), sin_z = std::sin(rz);
        
        float cos_c = std::cos(-cam_rot_y), sin_c = std::sin(-cam_rot_y);
        const float light_x = 0.5773f; const float light_y = 0.5773f; const float light_z = 0.5773f;

        for (uint32_t i = 0; i < mesh.vertex_count && i < 64; i++) {
            GeonVertex v = mesh.vertices[i];
            float x = v.x * scale_x; float y = v.y * scale_y; float z = v.z * scale_z;

            float y1 = y * cos_x - z * sin_x; float z1 = y * sin_x + z * cos_x;
            float x2 = x * cos_y + z1 * sin_y; float z2 = -x * sin_y + z1 * cos_y;
            float x3 = x2 * cos_z - y1 * sin_z; float y3 = x2 * sin_z + y1 * cos_z;

            float wx = x3 + tx; float wy = y3 + ty; float wz = z2 + tz;

            // Fully patched camera math utilizing pitch, translation, and local matrices
            float rx_cam = (wx - cx) * cos_c + (wz - cz) * sin_c;
            float rz_cam = -(wx - cx) * sin_c + (wz - cz) * cos_c;
            float ry_cam = wy - cy;

            world_x[i] = rx_cam;
            world_y[i] = ry_cam;
            world_z[i] = rz_cam + 3.5f; 

            if (world_z[i] > 0.1f) {
                screen_x[i] = static_cast<int>((world_x[i] / world_z[i]) * 240.0f + (FRAME_WIDTH / 2));
                screen_y[i] = static_cast<int>((-world_y[i] / world_z[i]) * 240.0f + (FRAME_HEIGHT / 2));
                vertex_valid[i] = true;
            } else {
                vertex_valid[i] = false;
            }
        }

        for (uint32_t i = 0; i < mesh.index_count; i += 3) {
            if (i + 2 >= mesh.index_count) break;
            uint16_t idx0 = mesh.indices[i]; uint16_t idx1 = mesh.indices[i + 1]; uint16_t idx2 = mesh.indices[i + 2];
            if (idx0 >= 64 || idx1 >= 64 || idx2 >= 64) continue;
            if (!vertex_valid[idx0] || !vertex_valid[idx1] || !vertex_valid[idx2]) continue;

            GeonVertex v0 = mesh.vertices[idx0];
            float ny1 = v0.ny * cos_x - v0.nz * sin_x; float nz1 = v0.ny * sin_x + v0.nz * cos_x;
            float nx2 = v0.nx * cos_y + nz1 * sin_y; float nz2 = -v0.nx * sin_y + nz1 * cos_y;
            float nx3 = nx2 * cos_z - ny1 * sin_z; float ny3 = nx2 * sin_z + ny1 * cos_z;

            float culling_dot = (nx3 * world_x[idx0]) + (ny3 * world_y[idx0]) + (nz2 * world_z[idx0]);
            if (culling_dot >= 0.0f) continue;

            float light_dot = (nx3 * light_x) + (ny3 * light_y) + (nz2 * light_z);
            if (light_dot < 0.0f) light_dot = 0.0f;

            float intensity = 0.20f + (light_dot * 0.80f);
            uint8_t shaded_color = static_cast<uint8_t>(base_color * intensity);

            draw_software_triangle(screen_x[idx0], screen_y[idx0], screen_x[idx1], screen_y[idx1], screen_x[idx2], screen_y[idx2], shaded_color);
        }
    }

    void refresh_simulated_matrix() {
    //tearing fix pause pre buffer
    
    esp_rom_delay_us(220); // 200 microsecond buffer zone

        memset(frame_buffer, 12, FRAME_BUFFER_SIZE); 

        rotation_angle += 0.015f; // Toned down object tumbles slightly to clean visual rates
        if (rotation_angle > 6.28318f) rotation_angle -= 6.28318f;

        if (!g_actors_initialized) {
            for (int i = 0; i < MAX_ACTORS; i++) {
        // SCATTER STEP: Expands the random spread out to a 36-unit zone matching your giant grid
        g_world_actors[i].x = (((float)(rand() % 100) / 100.0f) * 36.0f) - 18.0f;
        g_world_actors[i].y = ((float)(rand() % 100) / 100.0f) * 0.4f;
        g_world_actors[i].z = (((float)(rand() % 100) / 100.0f) * 36.0f) - 18.0f;
        
        g_world_actors[i].dx = (((float)(rand() % 100) / 100.0f) * 0.015f) - 0.0075f;
        g_world_actors[i].dy = 0.0f;
        g_world_actors[i].dz = (((float)(rand() % 100) / 100.0f) * 0.015f) - 0.0075f;
        g_world_actors[i].rotation = 0.0f;
        g_world_actors[i].base_color = 120 + (rand() % 135);
            }
            g_world_actors[0].x = 0.0f; g_world_actors[0].y = 0.0f; g_world_actors[0].z = 0.0f;
            g_world_actors[0].dx = 0.0f; g_world_actors[0].dy = 0.0f; g_world_actors[0].dz = 0.0f;
            g_world_actors[0].base_color = 240; 
            g_actors_initialized = true;
        }

        // Processing Player 1 (Geon 0) Vector Speed Step updates
        g_world_actors[0].rotation += g_input_turn * 0.035f; // Smooth rotation
        if (g_input_forward != 0.0f) {
            g_world_actors[0].x += std::sin(g_world_actors[0].rotation) * g_input_forward * 0.025f;
            g_world_actors[0].z += std::cos(g_world_actors[0].rotation) * g_input_forward * 0.025f;
        }

        for (int i = 0; i < MAX_ACTORS; i++) {
            if (i != 0) { 
                g_world_actors[i].x += g_world_actors[i].dx;
                g_world_actors[i].y += g_world_actors[i].dy;
                g_world_actors[i].z += g_world_actors[i].dz;
                g_world_actors[i].dy -= 0.0008f; // Smoothed grav step pull down

                if (g_world_actors[i].y < -0.48f) {
                    g_world_actors[i].y = -0.48f;
                    g_world_actors[i].dy = -g_world_actors[i].dy * 0.82f;
                }
                if (g_world_actors[i].x < -1.5f || g_world_actors[i].x > 1.5f) g_world_actors[i].dx = -g_world_actors[i].dx;
                if (g_world_actors[i].z < -1.5f || g_world_actors[i].z > 1.5f) g_world_actors[i].dz = -g_world_actors[i].dz;
            }
        }

        float player_rot = g_world_actors[0].rotation;
    // Updated to use your new HTML web container zoom factor
    float cam_dist = g_camera_zoom; 
    float cam_x = g_world_actors[0].x - std::sin(player_rot) * cam_dist;
    float cam_z = g_world_actors[0].z - std::cos(player_rot) * cam_dist;
    float cam_y = g_world_actors[0].y + 1.4f; 

        // Render ground base landscape plane relative to tracking vectors
// 1. Render primary ground base landscape plane (Page 11)
// Extended width and length scaling from 4.0f out to 6.5f 
render_geon_software(GEON_HEXAHEDRON, 0.0f, -0.55f, 0.0f, 0.0f, 0.0f, 0.0f, 65.0f, 0.01f, 65.0f, 75, player_rot, cam_x, cam_y, cam_z);

// 2. Thick Under-Water Geon Layer
// Stretched from 7.0f out to 70.0f
render_geon_software(GEON_HEXAHEDRON, 0.0f, -0.65f, 0.0f, 0.0f, 0.0f, 0.0f, 70.0f, 0.20f, 70.0f, 30, player_rot, cam_x, cam_y, cam_z);

if (!g_binary_map_loaded) {
    build_binary_tilemap();
}

// Loop through and stream your tiles to the screen buffer
SceneNode* current_tile = g_tilemap_head;
while (current_tile != nullptr) {
    render_geon_software(
        static_cast<GeonType>(current_tile->geon_type),
        current_tile->x, current_tile->y, current_tile->z,
        current_tile->rot_x, current_tile->rot_y, current_tile->rot_z,
        current_tile->scale_x, current_tile->scale_y, current_tile->scale_z,
        155, // Solid, bright terminal green color level for visible tile slabs
        player_rot, cam_x, cam_y, cam_z
    );
    current_tile = current_tile->next;
}

        // Render actor arrays sequentially
        for(int i = 0; i < MAX_ACTORS; i++) {
            render_geon_software(
                GEON_HEXAHEDRON, 
                g_world_actors[i].x, g_world_actors[i].y, g_world_actors[i].z,
                (i == 0) ? 0.0f : rotation_angle * 0.6f, (i == 0) ? player_rot : rotation_angle * 1.2f, 0.0f,
                (i == 0) ? 0.12f : 0.08f, (i == 0) ? 0.12f : 0.08f, (i == 0) ? 0.12f : 0.08f,
                g_world_actors[i].base_color, player_rot, cam_x, cam_y, cam_z
            );
        }
        esp_rom_delay_us(100);
        
    }
};

static VideoSystem g_video_subsystem;

void vEngineUpdateTask(void* pvParameters) {
    // Elevate priority dynamically if needed
    vTaskPrioritySet(NULL, 12);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    // CALIBRATE FOR 10 FPS: 100 milliseconds per frame cycle
    const TickType_t xFrequency = pdMS_TO_TICKS(100); 
    
    while(true) {
        g_video_subsystem.refresh_simulated_matrix();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

class BiosWebServer {
private:
    httpd_handle_t server_handle = nullptr;

const char* bios_html_ui = R"rawliteral(<!DOCTYPE html><html><head>
 <title>CODEX OS - Fallout Dev Terminal</title>
 <meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-select=none'>
 <style>
 body { background:#030305; color:#00ff55; font-family:monospace; margin:0; padding:10px; text-align:center; overflow:hidden;}
 #terminal-container { max-width: 600px; margin: 0 auto; position: relative; }
 canvas { background:#000; border:2px solid #00ff55; box-shadow:0 0 20px rgba(0,255,85,0.2); width:100%; max-width:480px; image-rendering: pixelated; }
 .control-panel { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; max-width: 480px; margin: 15px auto; padding: 5px; }
 .btn { background:#001100; color:#00ff55; border:2px solid #00ff55; padding:14px 5px; cursor:pointer; font-weight:bold; font-size:13px; text-transform:uppercase; font-family:monospace; user-select:none; touch-action: manipulation; }
 .btn:active { background:#00ff55; color:#000; }
 .dev-btn { border-color: #ffaa00; color: #ffaa00; background: #110800; }
 .dev-btn:active { background:#ffaa00; color:#000; }
 .select-style { background:#001100; color:#00ff55; border:2px solid #00ff55; font-family:monospace; padding:12px; font-weight:bold; width:100%; }
 .empty-slot { visibility: hidden; }
 </style></head><body>
 <div id='terminal-container'>
 <h3>CODEX CORE ENGINE V1.03</h3>
 <canvas id='glcanvas' width='320' height='240'></canvas>
 
 <div class='control-panel'>
 <button class='btn dev-btn' id='zoomin'>Zoom In</button>
 <button class='btn' id='fwd'>Forward</button>
 <button class='btn dev-btn' id='zoomout'>Zoom Out</button>
 
 <button class='btn' id='tl'>Turn Left</button>
 <button class='btn dev-btn' id='devmode'>Dev Mode</button>
 <button class='btn' id='tr'>Turn Right</button>
 
 <select class='select-style' id='geonSelect'>
   <option value='0'>Hexahedron (Cube)</option>
   <option value='1'>Wedge Prism</option>
   <option value='2'>Hex Prism</option>
   <option value='3'>Dodecahedron</option>
 </select>
 <button class='btn' id='bak'>Back</button>
 <button class='btn dev-btn' id='placeitem'>Place Item</button>
 
 <button class='btn' id='jmp'>Jump</button>
 <button class='btn' id='verb'>Do Verb</button>
 <button class='btn' id='chg'>Change Verb</button>
 </div>
 </div>
 <script>
 const canvas = document.getElementById('glcanvas');
 const ctx = canvas.getContext('2d');
 let accum = new Uint8Array(76800);
 function sendInput(cmd) { fetch('/input?set=' + cmd); }
 function setupInput(id, activeCmd, idleCmd) {
 const el = document.getElementById(id);
 el.addEventListener('mousedown', () => sendInput(activeCmd));
 el.addEventListener('mouseup', () => sendInput(idleCmd));
 el.addEventListener('touchstart', (e) => { e.preventDefault(); sendInput(activeCmd); });
 el.addEventListener('touchend', () => sendInput(idleCmd));
 }
 setupInput('fwd', 'F', 'S'); setupInput('bak', 'B', 'S');
 setupInput('tl', 'L', 'N'); setupInput('tr', 'R', 'N');
 document.getElementById('jmp').onclick = () => sendInput('J');
 document.getElementById('verb').onclick = () => sendInput('V');
 document.getElementById('chg').onclick = () => sendInput('C');
 
 // Hooking up new Dev Mode control listeners
 document.getElementById('zoomin').onclick = () => sendInput('Z');
 document.getElementById('zoomout').onclick = () => sendInput('O');
 document.getElementById('devmode').onclick = () => sendInput('D');
 document.getElementById('placeitem').onclick = () => sendInput('P');
 
 // Combobox change handler
 document.getElementById('geonSelect').onchange = (e) => {
     fetch('/input?geon=' + e.target.value);
 };

 async function updateFrameStream() {
 try {
 const res = await fetch('/stream');
 const reader = res.body.getReader();
 let pos = 0;
 while(true) {
 const {done, value} = await reader.read();
 if(done) break;
 accum.set(value, pos);
 pos += value.length;
 if(pos >= 76800) {
 const imgData = ctx.createImageData(320, 240);
for(let i=0; i<76800; i++) {
    let val = accum[i];
    
    // If the pixel matches the background clear color (12), make it pitch black
    if (val === 12) {
        imgData.data[i*4 + 0] = 0;   // Red
        imgData.data[i*4 + 1] = 0;   // Green
        imgData.data[i*4 + 2] = 0;   // Blue
    } else {
        // Otherwise, render objects with your bright green terminal color palette
        imgData.data[i*4 + 0] = 0; 
        imgData.data[i*4 + 1] = Math.min(255, (val << 1) + 50); // Scale object edges out cleanly
        imgData.data[i*4 + 2] = val; 
    }
    imgData.data[i*4 + 3] = 255; // Alpha channel (opaque)
}
 ctx.putImageData(imgData, 0, 0);
 pos = 0;
 }
 }
 setTimeout(updateFrameStream, 4);
 } catch(e) { setTimeout(updateFrameStream, 100); }
 }
 updateFrameStream();
 </script></body></html>)rawliteral";

    static esp_err_t root_ui_handler(httpd_req_t *req) {
        BiosWebServer* self = static_cast<BiosWebServer*>(req->user_ctx);
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, self->bios_html_ui, strlen(self->bios_html_ui));
        return ESP_OK;
    }

static esp_err_t input_handler(httpd_req_t *req) {
    char buf[32];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[16];
        if (httpd_query_key_value(buf, "set", param, sizeof(param)) == ESP_OK) {
            if (param[0] == 'F') g_input_forward = 1.0f;
            else if (param[0] == 'B') g_input_forward = -1.0f;
            else if (param[0] == 'S') g_input_forward = 0.0f;
            else if (param[0] == 'L') g_input_turn = 1.0f;
            else if (param[0] == 'R') g_input_turn = -1.0f;
            else if (param[0] == 'N') g_input_turn = 0.0f;
            else if (param[0] == 'J') ESP_LOGI("INPUT", "Action Button: JUMP");
            else if (param[0] == 'V') ESP_LOGI("INPUT", "Action Button: DO VERB");
            else if (param[0] == 'C') ESP_LOGI("INPUT", "Action Button: CHANGE VERB");
            
            // New Dev Controls Parse
            else if (param[0] == 'Z') { // Zoom In
                g_camera_zoom -= g_camera_zoom*0.99f;
                if (g_camera_zoom < 0.05f) g_camera_zoom = 0.05f;
            }
            else if (param[0] == 'O') { // Zoom Out
                g_camera_zoom += g_camera_zoom*1.01f;
                if (g_camera_zoom > 8.0f) g_camera_zoom = 8.0f;
            }
            else if (param[0] == 'D') { // Toggle Dev Mode
                g_dev_mode = !g_dev_mode;
                ESP_LOGI("DEV", "Dev Mode Toggled: %s", g_dev_mode ? "ON" : "OFF");
            }
            else if (param[0] == 'P') { // Populate Item on Map
                ESP_LOGI("DEV", "Placing Object Type: %d at Player Pos", g_selected_geon_type);
                // Future Implementation: Add node to your SceneNode linked list here
            }
        }
        // Handle dropdown selection (e.g., /input?geon=2)
        if (httpd_query_key_value(buf, "geon", param, sizeof(param)) == ESP_OK) {
            g_selected_geon_type = atoi(param);
            ESP_LOGI("DEV", "Selected Geon Primitive Adjusted to: %d", g_selected_geon_type);
        }
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

    static esp_err_t stream_binary_handler(httpd_req_t *req) {
        httpd_resp_set_type(req, "application/octet-stream");
        const size_t chunk_size = 4096;
        size_t bytes_remaining = FRAME_BUFFER_SIZE;
        const char* data_ptr = reinterpret_cast<const char*>(g_video_subsystem.frame_buffer);

        while (bytes_remaining > 0) {
            size_t to_send = (bytes_remaining > chunk_size) ? chunk_size : bytes_remaining;
            esp_err_t res = httpd_resp_send_chunk(req, data_ptr, to_send);
            if (res != ESP_OK) return ESP_FAIL;
            data_ptr += to_send;
            bytes_remaining -= to_send;
        }
        httpd_resp_send_chunk(req, NULL, 0); 
        return ESP_OK;
    }

public:
    BiosWebServer() = default;

    bool start() {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        
        
        config.server_port = 80;
        config.stack_size = 8192;
        config.ctrl_port = 32768;
config.task_priority = 2; //tearing fix test
config.core_id = 0; 
        if (httpd_start(&server_handle, &config) == ESP_OK) {
            httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_ui_handler, .user_ctx = this };
            httpd_register_uri_handler(server_handle, &root_uri);

            httpd_uri_t input_uri = { .uri = "/input", .method = HTTP_GET, .handler = input_handler, .user_ctx = this };
            httpd_register_uri_handler(server_handle, &input_uri);

            httpd_uri_t data_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_binary_handler, .user_ctx = this };
            httpd_register_uri_handler(server_handle, &data_uri);
            return true;
        }
        return false;
    }
};

static BiosWebServer web_server;

extern "C" void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    initialize_wifi_station();

    static ClockScanner scanner(PIXEL_CLOCK_GPIO);
    scanner.activate();

    xTaskCreatePinnedToCore(vEngineUpdateTask, "EngineTask", 6144, NULL, 10, NULL, 0);

    if (web_server.start()) {
        ESP_LOGI("SYS_BOOT", "CODEX Graphics Engine Subsystem online.");
    }
}
