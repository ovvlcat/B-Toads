#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>

using namespace std;

// Секция данных игры  
typedef struct
{
    float x, y, width, height, speed, gravity, jumpForce, velocityY;
    HBITMAP hBitmap;     // хэндл к спрайту
    bool isJumping;      // флаг прыжка
    bool isOnGround;     // на земле ли
    bool wasOnGround;    // был ли на земле в предыдущем кадре
} Sprite;

typedef struct // структура для платформ
{
    float x, y, width, height;
    HBITMAP hBitmap;
    bool isTeleport;     // является ли телепортом
    int targetLocation;  // целевая локация для телепорта
} Platform;

typedef struct // структура для локации
{
    float groundLevel;           // уровень земли
    vector<Platform> platforms;  // платформы в локации
    HBITMAP background;          // фон локации
    int id;                      // идентификатор локации
} Location;

// Глобальные переменные
Sprite player;           // игрок
vector<Location> locations; // все локации
int currentLocation = 0; // текущая локация

struct
{
    bool action = false; // состояние - ожидание (игрок должен нажать пробел) или игра
} Game;

struct
{
    HWND hWnd;           // хэндл окна
    HDC device_context, context; // два контекста устройства (для буферизации)
    int width, height;   // сюда сохраним размеры окна
} Window;

// Время для дельта-тайма
auto lastTime = chrono::high_resolution_clock::now();

// Секция кода

// Функция проверки загрузки изображения (Unicode версия)
HBITMAP LoadBitmapSafe(const wchar_t* filename)
{
    HBITMAP hBitmap = (HBITMAP)LoadImageW(NULL, filename, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (!hBitmap)
    {
        wchar_t errorMsg[256];
        swprintf_s(errorMsg, L"Failed to load image: %s", filename);
        MessageBoxW(NULL, errorMsg, L"Error", MB_OK | MB_ICONERROR);
    }
    return hBitmap;
}

// Функция для преобразования int в wstring
wstring IntToWString(int value)
{
    wstringstream ws;
    ws << value;
    return ws.str();
}

// Функция для преобразования float в wstring
wstring FloatToWString(float value)
{
    wstringstream ws;
    ws << value;
    return ws.str();
}

// Инициализация игры
void InitGame()
{
    // Загружаем спрайт игрока
    player.hBitmap = LoadBitmapSafe(L"rash.bmp");

    // Инициализация игрока
    player.width = 128;
    player.height = 210;
    player.speed = 10.0f;
    player.gravity = 1.5f; // Увеличена гравитация для более быстрого падения
    player.jumpForce = 30.0f; // Увеличена сила прыжка
    player.velocityY = 0;
    player.isJumping = false;
    player.isOnGround = false;
    player.wasOnGround = false;

    // Устанавливаем уровень земли в два раза ниже (примерно на середине экрана)
    float groundLevel = Window.height / 2;

    player.x = Window.width / 2.0f;
    player.y = groundLevel - player.height; // Игрок стоит на земле

    // Создаем первую локацию
    Location loc1;
    loc1.id = 0;
    loc1.groundLevel = groundLevel;
    loc1.background = LoadBitmapSafe(L"back1.bmp");

    // Платформы с фиксированной высотой
    Platform plat1;
    plat1.width = 300;
    plat1.height = 30;
    plat1.x = 200;
    plat1.y = 250;

    Platform plat2;
    plat2.width = 200;
    plat2.height = 30;
    plat2.x = 600;
    plat2.y = 150;

    Platform plat3;
    plat3.width = 250;
    plat3.height = 30;
    plat3.x = 400;
    plat3.y = 350;

    plat1.hBitmap = NULL;
    plat1.isTeleport = false;
    plat1.targetLocation = 0;

    plat2.hBitmap = NULL;
    plat2.isTeleport = false;
    plat2.targetLocation = 0;

    plat3.hBitmap = NULL;
    plat3.isTeleport = false;
    plat3.targetLocation = 0;

    loc1.platforms.push_back(plat1);
    loc1.platforms.push_back(plat2);
    loc1.platforms.push_back(plat3);

    // Создаем вторую локацию
    Location loc2;
    loc2.id = 1;
    loc2.groundLevel = groundLevel;
    loc2.background = LoadBitmapSafe(L"back2.bmp");

    // Платформы во второй локации с фиксированной высотой
    Platform plat4;
    plat4.width = 350;
    plat4.height = 30;
    plat4.x = 100;
    plat4.y = 200;

    Platform plat5;
    plat5.width = 180;
    plat5.height = 30;
    plat5.x = 500;
    plat5.y = 300;

    Platform plat6;
    plat6.width = 220;
    plat6.height = 30;
    plat6.x = 300;
    plat6.y = 100;

    plat4.hBitmap = NULL;
    plat4.isTeleport = false;
    plat4.targetLocation = 0;

    plat5.hBitmap = NULL;
    plat5.isTeleport = false;
    plat5.targetLocation = 0;

    plat6.hBitmap = NULL;
    plat6.isTeleport = false;
    plat6.targetLocation = 0;

    loc2.platforms.push_back(plat4);
    loc2.platforms.push_back(plat5);
    loc2.platforms.push_back(plat6);

    locations.push_back(loc1);
    locations.push_back(loc2);
}

// Функция проверки коллизии AABB
bool CheckCollision(float x1, float y1, float w1, float h1,
    float x2, float y2, float w2, float h2)
{
    return (x1 < x2 + w2 &&
        x1 + w1 > x2 &&
        y1 < y2 + h2 &&
        y1 + h1 > y2);
}

// Улучшенная функция проверки коллизии с платформами
void CheckPlatformCollisions()
{
    Location& loc = locations[currentLocation];

    // Сохраняем предыдущее состояние
    player.wasOnGround = player.isOnGround;

    // Сначала проверяем, находимся ли мы уже на земле
    bool wasOnPlatform = player.isOnGround;
    player.isOnGround = false;

    // Проверяем столкновение с землей
    if (player.y >= loc.groundLevel - player.height)
    {
        player.y = loc.groundLevel - player.height;
        player.isOnGround = true;
        player.isJumping = false;
        player.velocityY = 0;
        return;
    }

    // Проверяем столкновение с платформами
    for (auto& platform : loc.platforms)
    {
        // Проверяем, пересекается ли игрок с платформой по X
        bool xCollision = (player.x < platform.x + platform.width) &&
            (player.x + player.width > platform.x);

        if (xCollision)
        {
            float playerBottom = player.y + player.height;
            float playerTop = player.y;
            float platformTop = platform.y;
            float platformBottom = platform.y + platform.height;

            // Проверяем, находится ли игрок над платформой
            bool isAbovePlatform = (playerBottom > platformTop) &&
                (playerTop < platformTop);

            // Проверяем, находится ли игрок под платформой
            bool isBelowPlatform = (playerTop < platformBottom) &&
                (playerBottom > platformBottom);

            // Если игрок над платформой и падает вниз
            if (isAbovePlatform && player.velocityY >= 0)
            {
                // Рассчитываем расстояние до платформы
                float distanceToPlatform = playerBottom - platformTop;

                // Если мы достаточно близко к платформе сверху (и падаем или стоим)
                if (distanceToPlatform >= 0 && distanceToPlatform < 30.0f)
                {
                    // Проверяем, не пытаемся ли мы пройти сквозь платформу снизу
                    if (player.velocityY >= 0) // Падаем вниз
                    {
                        player.y = platformTop - player.height;
                        player.isOnGround = true;
                        player.isJumping = false;
                        player.velocityY = 0;
                        return;
                    }
                }
            }
            // Если игрок под платформой и прыгает вверх - игнорируем (можно пройти сквозь снизу)
        }
    }
}

// Проверка перехода в следующую локацию с ограничениями
void CheckLocationTransition()
{
    // Локация 0 -> Локация 1 только через правую сторону
    if (currentLocation == 0)
    {
        if (player.x > Window.width - player.width / 2)
        {
            currentLocation = 1;
            player.x = 0;
            player.y = locations[currentLocation].groundLevel - player.height;
            player.isOnGround = true;
            player.isJumping = false;
            player.velocityY = 0;
            player.wasOnGround = true;
        }
        // Упираемся в левый край
        else if (player.x < 0)
        {
            player.x = 0;
        }
    }
    // Локация 1 -> Локация 0 только через левую сторону
    else if (currentLocation == 1)
    {
        if (player.x < -player.width / 2)
        {
            currentLocation = 0;
            player.x = Window.width - player.width;
            player.y = locations[currentLocation].groundLevel - player.height;
            player.isOnGround = true;
            player.isJumping = false;
            player.velocityY = 0;
            player.wasOnGround = true;
        }
        // Упираемся в правый край
        else if (player.x > Window.width - player.width)
        {
            player.x = Window.width - player.width;
        }
    }
}

// Прыжок - улучшенная версия
void StartJump()
{
    if ((player.isOnGround || player.wasOnGround) && !player.isJumping)
    {
        player.isJumping = true;
        player.isOnGround = false;
        player.velocityY = -player.jumpForce;
    }
}

// Отображение отладочной информации
void ShowScore()
{
    SetTextColor(Window.context, RGB(255, 255, 255));
    SetBkColor(Window.context, RGB(0, 0, 0));
    SetBkMode(Window.context, TRANSPARENT);

    // Используем шрифт по умолчанию для Unicode
    auto hFont = CreateFontW(20, 0, 0, 0, FW_NORMAL, 0, 0, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH, L"Arial");
    auto hOldFont = (HFONT)SelectObject(Window.context, hFont);

    // Позиция игрока
    wstring playerPos = L"X: " + FloatToWString(player.x) + L" Y: " + FloatToWString(player.y);
    TextOutW(Window.context, 10, 10, playerPos.c_str(), (int)playerPos.length());

    // Состояние игрока
    wstring stateStr = L"State: " + wstring(player.isOnGround ? L"On Ground" : L"In Air");
    TextOutW(Window.context, 10, 35, stateStr.c_str(), (int)stateStr.length());

    // Скорость
    wstring velocityStr = L"VelocityY: " + FloatToWString(player.velocityY);
    TextOutW(Window.context, 10, 60, velocityStr.c_str(), (int)velocityStr.length());

    // Было ли на земле
    wstring wasGroundStr = L"WasGround: " + wstring(player.wasOnGround ? L"Yes" : L"No");

    SelectObject(Window.context, hOldFont);
    DeleteObject(hFont);
}

// Обработка ввода с дельта-таймом
void ProcessInput(float deltaTime)
{
    // Движение влево/вправо с учетом дельта-тайма
    float moveSpeed = player.speed * deltaTime * 60.0f;

    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
        player.x -= moveSpeed;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
        player.x += moveSpeed;

    // Прыжок (только при нажатии, не удержании)
    static bool upKeyPressed = false;
    bool upKeyCurrent = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;

    if (upKeyCurrent && !upKeyPressed)
    {
        StartJump();
    }
    upKeyPressed = upKeyCurrent;
}

// Отображение битмапа
void ShowBitmap(HDC hDC, int x, int y, int w, int h, HBITMAP hBitmap)
{
    if (!hBitmap) return;

    HDC hMemDC = CreateCompatibleDC(hDC);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBitmap);

    BITMAP bm;
    GetObject(hBitmap, sizeof(BITMAP), &bm);

    StretchBlt(hDC, x, y, w, h, hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

    SelectObject(hMemDC, hOldBmp);
    DeleteDC(hMemDC);
}

// Отображение платформ (упрощенная текстура - один цвет)
void ShowPlatforms()
{
    Location& loc = locations[currentLocation];

    // Рисуем платформы одним простым цветом
    for (auto& platform : loc.platforms)
    {
        // Создаем простую кисть одного цвета для платформ
        HBRUSH hBrush = CreateSolidBrush(RGB(139, 69, 19)); // Коричневый цвет
        HBRUSH hOldBrush = (HBRUSH)SelectObject(Window.context, hBrush);

        // Рисуем прямоугольник платформы
        Rectangle(Window.context,
            (int)platform.x, (int)platform.y,
            (int)(platform.x + platform.width),
            (int)(platform.y + platform.height));

        SelectObject(Window.context, hOldBrush);
        DeleteObject(hBrush);
    }
}

// Отображение игры
void ShowGame()
{
    // Очищаем экран
    HBRUSH hBlackBrush = CreateSolidBrush(RGB(0, 0, 0));
    RECT rect = { 0, 0, Window.width, Window.height };
    FillRect(Window.context, &rect, hBlackBrush);
    DeleteObject(hBlackBrush);

    // Фон локации
    Location& loc = locations[currentLocation];
    if (loc.background)
    {
        ShowBitmap(Window.context, 0, 0, Window.width, Window.height, loc.background);
    }

    // Платформы (упрощенный вид)
    ShowPlatforms();

    // Игрок
    ShowBitmap(Window.context, (int)player.x, (int)player.y,
        (int)player.width, (int)player.height, player.hBitmap);
}

// Ограничение игрока в пределах экрана
void LimitPlayer()
{
    // Не даем игроку упасть ниже нижней границы
    player.y = min(player.y, (float)Window.height - player.height);

    // Проверяем переход в следующую локацию с правилами
    CheckLocationTransition();
}

// Обновление физики с дельта-таймом
void UpdatePhysics(float deltaTime)
{
    // Применяем гравитацию с учетом дельта-тайма
    if (!player.isOnGround)
    {
        player.velocityY += player.gravity * deltaTime * 60.0f;
        player.y += player.velocityY * deltaTime * 60.0f;
    }

    // Проверяем коллизии
    CheckPlatformCollisions();

    // Ограничиваем позицию игрока
    LimitPlayer();
}

// Инициализация окна
void InitWindow()
{
    SetProcessDPIAware();

    // Получаем размеры экрана
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Создаем полноэкранное окно
    Window.hWnd = CreateWindowW(L"edit", L"Platformer Game - Improved Collisions",
        WS_POPUP | WS_VISIBLE,
        0, 0, screenWidth, screenHeight,
        NULL, NULL, NULL, NULL);

    // Получаем размеры клиентской области
    RECT clientRect;
    GetClientRect(Window.hWnd, &clientRect);
    Window.width = clientRect.right - clientRect.left;
    Window.height = clientRect.bottom - clientRect.top;

    // Создаем контексты устройств
    Window.device_context = GetDC(Window.hWnd);
    Window.context = CreateCompatibleDC(Window.device_context);

    // Создаем совместимый битмап для буфера
    HBITMAP hBufferBmp = CreateCompatibleBitmap(Window.device_context,
        Window.width, Window.height);
    SelectObject(Window.context, hBufferBmp);
}

// Точка входа Windows приложения
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    // Инициализация
    InitWindow();
    InitGame();

    // Скрываем курсор
    ShowCursor(FALSE);

    // Основной игровой цикл
    while (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000))
    {
        // Расчет дельта-тайма
        auto currentTime = chrono::high_resolution_clock::now();
        chrono::duration<float> deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        float delta = deltaTime.count();

        // Ограничиваем дельта-тайм для стабильности
        if (delta > 0.1f) delta = 0.1f;

        // Обработка ввода с дельта-таймом
        ProcessInput(delta);

        // Обновление физики с дельта-таймом
        UpdatePhysics(delta);

        // Отрисовка
        ShowGame();
        ShowScore();

        // Копируем буфер на экран
        BitBlt(Window.device_context, 0, 0, Window.width, Window.height,
            Window.context, 0, 0, SRCCOPY);

        // Небольшая задержка для стабильности
        Sleep(1);
    }

    // Очистка ресурсов
    ReleaseDC(Window.hWnd, Window.device_context);
    DeleteDC(Window.context);
    DestroyWindow(Window.hWnd);

    return 0;
}