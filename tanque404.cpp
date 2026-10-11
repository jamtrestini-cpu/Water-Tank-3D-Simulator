/*
    TANQUE 404 - Simulador 3D
    C++ + Raylib, archivo unico: tanque404.cpp

    Requisitos:
      - C++17
      - Raylib instalada y enlazada con el compilador.
    Compilacion (ejemplo con MinGW si Raylib esta configurada):
      g++ tanque404.cpp -o tanque404 -std=c++17 -lraylib -lopengl32 -lgdi32 -lwinmm

    Nota: esta version usa C++ y Raylib, por lo que NO cumple las restricciones
    de la entrega C99 original. Es una alternativa visual independiente.
*/

#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>

static constexpr int SCREEN_W = 1280;
static constexpr int SCREEN_H = 800;
static constexpr float TANK_CAPACITY = 1000.0f;
static constexpr int TOTAL_DAYS = 30;

struct Simulation {
    float level = 600.0f;
    float consumedTotal = 0.0f;
    float receivedTotal = 0.0f;
    float overflowTotal = 0.0f;
    float missingTotal = 0.0f;
    float dayConsumption = 0.0f;
    float maxDayConsumption = 0.0f;
    int maxDay = 0;
    int rationDays = 0;
    int dryDays = 0;
    int noPipeStreak = 0;
    int maxNoPipeStreak = 0;
    int cisterns = 0;
    int day = 1;
    int turn = 0;
    int menu = 0;
    bool waterToday = false;
    bool waterDecided = false;
    bool cisternUsedToday = false;
    bool rationedToday = false;
    bool dryToday = false;
    bool dayFinished = false;
    bool report = false;
    bool running = true;
    bool professorMode = false;
    std::string status = "Listo para comenzar";
    float targetLevel = 600.0f;
    float animationTimer = 0.0f;
};

static const char* TurnName(int turn) {
    switch (turn) {
        case 0: return "MANANA";
        case 1: return "TARDE";
        default: return "NOCHE";
    }
}

static Color LevelColor(float level) {
    const float p = level / TANK_CAPACITY;
    if (p < 0.20f) return RED;
    if (p < 0.40f) return GOLD;
    return SKYBLUE;
}

static int RandomInt(int low, int high) {
    return low + std::rand() % (high - low + 1);
}

static void DrawTextShadow(const char* text, int x, int y, int size, Color color) {
    DrawText(text, x + 2, y + 2, size, Fade(BLACK, 0.6f));
    DrawText(text, x, y, size, color);
}

static void DrawTank3D(float level, float displayedLevel, bool waterComing, int frame) {
    const Vector3 tankPos = {0.0f, 1.8f, 0.0f};
    const float tankHeight = 3.8f;
    const float tankWidth = 3.0f;
    const float fill = std::clamp(displayedLevel / TANK_CAPACITY, 0.0f, 1.0f);
    const float waterHeight = tankHeight * fill;
    const Color waterColor = LevelColor(displayedLevel);

    // Base y in 3D is at 0; the tank's center is half its height.
    DrawCube({tankPos.x, tankHeight / 2.0f, tankPos.z},
             tankWidth, tankHeight, tankWidth,
             Fade(LIGHTGRAY, 0.18f));
    DrawCubeWires({tankPos.x, tankHeight / 2.0f, tankPos.z},
                  tankWidth, tankHeight, tankWidth, RAYWHITE);

    if (waterHeight > 0.02f) {
        DrawCube({0.0f, waterHeight / 2.0f, 0.0f},
                 tankWidth * 0.94f, waterHeight, tankWidth * 0.94f,
                 Fade(waterColor, 0.82f));
        // A thin wave surface makes the current waterline easy to read.
        DrawCube({0.0f, waterHeight + 0.015f * std::sin(frame * 0.15f), 0.0f},
                 tankWidth * 0.95f, 0.035f, tankWidth * 0.95f,
                 Fade(WHITE, 0.42f));
    }

    // Metallic bands around the tank.
    for (int i = 1; i <= 4; ++i) {
        const float y = tankHeight * i / 5.0f;
        DrawCubeWires({0.0f, y, 0.0f}, tankWidth * 1.01f, 0.025f,
                      tankWidth * 1.01f, Fade(DARKGRAY, 0.85f));
    }

    // Inlet pipe and animated drops when pipe water is available.
    DrawCube({-1.95f, 3.45f, 0.0f}, 1.1f, 0.16f, 0.16f, GRAY);
    DrawCube({-2.45f, 3.05f, 0.0f}, 0.16f, 0.9f, 0.16f, GRAY);
    if (waterComing) {
        for (int i = 0; i < 5; ++i) {
            const float dy = std::fmod((frame * 0.045f + i * 0.31f), 0.9f);
            DrawSphere({-2.45f, 2.65f - dy, 0.0f}, 0.055f, SKYBLUE);
        }
    }

    // Small drain outlet.
    DrawCube({1.65f, 0.28f, 0.0f}, 0.45f, 0.12f, 0.18f, DARKGRAY);
    DrawSphere({1.92f, 0.24f, 0.0f}, 0.07f, waterComing ? SKYBLUE : GRAY);
}

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_W, SCREEN_H, "TANQUE 404 - Simulador 3D");
    SetTargetFPS(60);

    Simulation sim;
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    Camera3D camera = {};
    camera.position = {7.0f, 6.0f, 8.0f};
    camera.target = {0.0f, 1.7f, 0.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float displayedLevel = sim.level;
    int frame = 0;
    bool showHelp = true;
    float cameraYaw = 0.72f;
    float cameraPitch = 0.48f;
    float cameraDistance = 10.7f;
    bool pendingWater = false;
    float pendingAdded = 0.0f;
    float pendingConsumed = 0.0f;

    while (!WindowShouldClose() && sim.running) {
        const float dt = GetFrameTime();
        frame++;

        if (IsKeyPressed(KEY_R)) {
            sim = Simulation{};
            displayedLevel = sim.level;
            pendingWater = false;
            pendingAdded = pendingConsumed = 0.0f;
        }
        if (IsKeyPressed(KEY_H)) showHelp = !showHelp;
        if (IsKeyPressed(KEY_ESCAPE)) sim.running = false;

        // Camara orbital controlada con el raton:
        // mantener el boton izquierdo y arrastrar para girar alrededor del tanque.
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 mouseDelta = GetMouseDelta();
            cameraYaw -= mouseDelta.x * 0.006f;
            cameraPitch += mouseDelta.y * 0.006f;

            // Limita la inclinacion para evitar que la camara se voltee.
            if (cameraPitch < 0.08f) cameraPitch = 0.08f;
            if (cameraPitch > 1.35f) cameraPitch = 1.35f;
        }

        // Rueda del raton para acercar o alejar la vista.
        cameraDistance -= GetMouseWheelMove() * 0.8f;
        if (cameraDistance < 5.0f) cameraDistance = 5.0f;
        if (cameraDistance > 18.0f) cameraDistance = 18.0f;

        camera.position.x = camera.target.x + cameraDistance * std::cos(cameraPitch) * std::sin(cameraYaw);
        camera.position.y = camera.target.y + cameraDistance * std::sin(cameraPitch);
        camera.position.z = camera.target.z + cameraDistance * std::cos(cameraPitch);

        // Animate the visible water level toward the actual simulation level.
        displayedLevel += (sim.level - displayedLevel) * std::min(1.0f, dt * 2.4f);

        if (IsKeyPressed(KEY_ONE)) sim.menu = 1;
        if (IsKeyPressed(KEY_TWO)) sim.menu = 2;
        if (IsKeyPressed(KEY_THREE)) sim.menu = 3;
        if (IsKeyPressed(KEY_F1)) sim.report = !sim.report;

        if (!sim.report && sim.day <= TOTAL_DAYS) {
            if (sim.menu == 1) {
                sim.menu = 0;
                if (sim.turn == 0 && !sim.waterDecided) {
                    sim.waterToday = (RandomInt(1, 100) <= 40);
                    sim.waterDecided = true;
                    sim.cisternUsedToday = false;
                    sim.rationedToday = false;
                    sim.dryToday = false;
                    sim.dayConsumption = 0.0f;
                }

                float incoming = sim.waterToday ? static_cast<float>(RandomInt(80, 200)) : 0.0f;
                float before = sim.level;
                sim.level += incoming;
                sim.receivedTotal += incoming;
                if (sim.level > TANK_CAPACITY) {
                    sim.overflowTotal += sim.level - TANK_CAPACITY;
                    sim.level = TANK_CAPACITY;
                    sim.status = "DESBORDE: agua perdida";
                }

                float use = 0.0f;
                if (sim.turn == 0) use = static_cast<float>(RandomInt(60, 120));
                else if (sim.turn == 1) use = static_cast<float>(RandomInt(40, 90));
                else use = static_cast<float>(RandomInt(20, 60));

                if (sim.level < 200.0f) {
                    use *= 0.5f;
                    if (!sim.rationedToday) {
                        sim.rationDays++;
                        sim.rationedToday = true;
                    }
                }
                sim.dayConsumption += use;
                sim.consumedTotal += use;
                sim.level -= use;
                if (sim.level < 0.0f) {
                    sim.missingTotal += -sim.level;
                    sim.level = 0.0f;
                    if (!sim.dryToday) {
                        sim.dryDays++;
                        sim.dryToday = true;
                    }
                    sim.status = "TANQUE SECO: no alcanza el agua";
                } else if (sim.level < 200.0f) {
                    sim.status = "RACIONAR AGUA";
                } else {
                    sim.status = incoming > 0.0f ? "ENTRADA DE AGUA" : "Consumo normal";
                }
                pendingWater = incoming > 0.0f;
                pendingAdded = incoming;
                pendingConsumed = use;

                sim.turn++;
                if (sim.turn >= 3) {
                    sim.turn = 0;
                    sim.dayFinished = true;
                    if (!sim.waterToday) {
                        sim.noPipeStreak++;
                        sim.maxNoPipeStreak = std::max(sim.maxNoPipeStreak, sim.noPipeStreak);
                    } else sim.noPipeStreak = 0;

                    if (sim.dayConsumption > sim.maxDayConsumption) {
                        sim.maxDayConsumption = sim.dayConsumption;
                        sim.maxDay = sim.day;
                    }
                    sim.day++;
                    sim.waterDecided = false;
                    sim.dayFinished = false;
                    if (sim.day > TOTAL_DAYS) {
                        sim.day = TOTAL_DAYS;
                        sim.report = true;
                        sim.status = "Simulacion finalizada";
                    }
                }
            } else if (sim.menu == 2) {
                sim.menu = 0;
                if (!sim.cisternUsedToday) {
                    const float beforeCistern = sim.level;
                    sim.level += 500.0f;
                    sim.receivedTotal += 500.0f;
                    if (sim.level > TANK_CAPACITY) {
                        sim.overflowTotal += sim.level - TANK_CAPACITY;
                        sim.level = TANK_CAPACITY;
                    }
                    sim.cisterns++;
                    sim.cisternUsedToday = true;
                    sim.status = "CISTERNA: +500 L; posible desborde; coste $30";
                } else {
                    sim.status = "Ya se uso la cisterna hoy";
                }
            } else if (sim.menu == 3) {
                sim.menu = 0;
                sim.report = true;
            }
        }

        BeginDrawing();
        ClearBackground({15, 23, 42, 255});

        // Header
        DrawRectangleGradientV(0, 0, GetScreenWidth(), 112, {15, 23, 42, 255}, {30, 41, 59, 255});
        DrawTextShadow("TANQUE 404", 32, 20, 34, RAYWHITE);
        DrawText("SIMULADOR DE AGUA - VISTA 3D", 35, 62, 16, SKYBLUE);
        DrawText(TextFormat("DIA %02d / %02d", sim.day, TOTAL_DAYS), GetScreenWidth() - 250, 24, 24, RAYWHITE);
        DrawText(TextFormat("TURNO: %s", TurnName(sim.turn)), GetScreenWidth() - 250, 59, 18, LIGHTGRAY);

        if (!sim.report) {
            // 3D scene
            BeginMode3D(camera);
            DrawPlane({0.0f, -0.03f, 0.0f}, {12.0f, 12.0f}, {35, 48, 65, 255});
            DrawGrid(20, 1.0f);
            DrawTank3D(sim.level, displayedLevel, pendingWater, frame);
            EndMode3D();

            // Side information panel
            DrawRectangleRounded({875, 145, 365, 475}, 0.04f, 8, {24, 35, 54, 235});
            DrawText("ESTADO DEL TANQUE", 900, 165, 20, RAYWHITE);
            DrawText(TextFormat("Nivel: %.0f / %.0f L", sim.level, TANK_CAPACITY), 900, 205, 22, LevelColor(sim.level));
            DrawText(TextFormat("Ocupacion: %.1f%%", sim.level / TANK_CAPACITY * 100.0f), 900, 240, 18, LIGHTGRAY);

            DrawRectangle(900, 280, 300, 20, DARKGRAY);
            DrawRectangle(900, 280, static_cast<int>(300 * sim.level / TANK_CAPACITY), 20, LevelColor(sim.level));
            DrawText(TextFormat("Consumido: %.0f L", sim.consumedTotal), 900, 325, 18, LIGHTGRAY);
            DrawText(TextFormat("Recibido: %.0f L", sim.receivedTotal), 900, 355, 18, LIGHTGRAY);
            DrawText(TextFormat("Desbordado: %.0f L", sim.overflowTotal), 900, 385, 18, LIGHTGRAY);
            DrawText(TextFormat("Cisternas: %d  |  Gasto: $%d", sim.cisterns, sim.cisterns * 30), 900, 415, 18, LIGHTGRAY);
            DrawText(TextFormat("Dias con racionamiento: %d", sim.rationDays), 900, 450, 16, LIGHTGRAY);
            DrawText(TextFormat("Dias secos: %d", sim.dryDays), 900, 478, 16, LIGHTGRAY);
            DrawText(TextFormat("Racha sin tuberia: %d", sim.maxNoPipeStreak), 900, 506, 16, LIGHTGRAY);

            DrawRectangleRounded({875, 545, 365, 75}, 0.05f, 8, {15, 23, 42, 230});
            DrawText("ESTADO", 895, 555, 14, GRAY);
            DrawText(sim.status.c_str(), 895, 580, 16, (sim.level < 200.0f ? RED : SKYBLUE));
        } else {
            // Final report screen
            DrawRectangleRounded({90, 140, GetScreenWidth() - 180.0f, 540}, 0.03f, 12, {24, 35, 54, 255});
            DrawText("REPORTE DE LA SIMULACION", 125, 170, 28, RAYWHITE);
            DrawText(TextFormat("Consumo total: %.0f L", sim.consumedTotal), 130, 225, 21, LIGHTGRAY);
            DrawText(TextFormat("Agua recibida: %.0f L", sim.receivedTotal), 130, 265, 21, LIGHTGRAY);
            DrawText(TextFormat("Agua desbordada: %.0f L", sim.overflowTotal), 130, 305, 21, LIGHTGRAY);
            DrawText(TextFormat("Litros que faltaron: %.0f L", sim.missingTotal), 130, 345, 21, LIGHTGRAY);
            DrawText(TextFormat("Dia de mayor consumo: %d (%.0f L)", sim.maxDay, sim.maxDayConsumption), 130, 385, 21, LIGHTGRAY);
            DrawText(TextFormat("Promedio diario: %.1f L", sim.consumedTotal / std::max(1, TOTAL_DAYS)), 130, 425, 21, LIGHTGRAY);
            DrawText(TextFormat("Dias con racionamiento: %d", sim.rationDays), 130, 465, 21, LIGHTGRAY);
            DrawText(TextFormat("Dias secos: %d", sim.dryDays), 130, 505, 21, LIGHTGRAY);
            DrawText(TextFormat("Racha mas larga sin agua: %d dias", sim.maxNoPipeStreak), 130, 545, 21, LIGHTGRAY);

            // Three-bar comparison graph.
            const int baseY = 640;
            const int maxWidth = 400;
            const float maxVal = std::max({sim.consumedTotal, sim.receivedTotal, sim.overflowTotal, 1.0f});
            DrawText("CONSUMO / RECIBIDO / DESBORDE", 720, 225, 18, RAYWHITE);
            const float vals[3] = {sim.consumedTotal, sim.receivedTotal, sim.overflowTotal};
            const char* labels[3] = {"Consumido", "Recibido", "Desbordado"};
            const Color cols[3] = {ORANGE, SKYBLUE, RED};
            for (int i = 0; i < 3; ++i) {
                int w = static_cast<int>(maxWidth * vals[i] / maxVal);
                DrawText(labels[i], 720, 270 + i * 100, 18, LIGHTGRAY);
                DrawRectangle(720, 300 + i * 100, w, 26, cols[i]);
                DrawText(TextFormat("%.0f L", vals[i]), 730 + w, 303 + i * 100, 18, RAYWHITE);
            }
            DrawText("R para reiniciar | H para ayuda | ESC salir", 130, 625, 16, GRAY);
        }

        // Controls / footer
        DrawRectangle(0, GetScreenHeight() - 90, GetScreenWidth(), 90, {8, 13, 25, 245});
        DrawText("1 = Simular turno   2 = Pedir cisterna   3 = Reporte", 32, GetScreenHeight() - 62, 18, RAYWHITE);
        DrawText("Arrastrar boton izquierdo = girar camara   Rueda = zoom   R = Reiniciar   ESC = Salir",
                 32, GetScreenHeight() - 32, 15, GRAY);

        if (showHelp) {
            DrawRectangleRounded({30, 130, 340, 150}, 0.04f, 8, {8, 13, 25, 230});
            DrawText("CONTROLES", 48, 145, 18, SKYBLUE);
            DrawText("1: simula un turno", 48, 177, 16, RAYWHITE);
            DrawText("2: solicita cisterna", 48, 201, 16, RAYWHITE);
            DrawText("3: abre el reporte", 48, 225, 16, RAYWHITE);
            DrawText("H: oculta esta ayuda", 48, 249, 16, RAYWHITE);
            DrawText("Raton: girar vista; rueda: zoom", 48, 273, 15, RAYWHITE);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
