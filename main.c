#include <efi.h>
#include <efilib.h>

#define OK      0
#define QUIT    1
#define DIED    1
#define LIVES   0

struct Pair{
    int x, y;
};

#define UP      ((struct Pair){.x =  0, .y = -1})
#define DOWN    ((struct Pair){.x =  0, .y =  1})
#define LEFT    ((struct Pair){.x = -1, .y =  0})
#define RIGHT   ((struct Pair){.x =  1, .y =  0})

struct BoardData{
    int width;
    int height;
    UINT32 color1;
    UINT32 color2;
    int segmentSize;
    struct Pair target;
    UINT32 targetColor;
    bool targetAlive;
};

struct Vector{
    struct Pair* data;
    int capacity;
    int size;
};

struct Snake{
    struct Vector segments;
    struct Pair direction;
    struct Pair previousDirection;
    UINT32 color;
};

void push_back(EFI_SYSTEM_TABLE *SystemTable, struct Vector *snake, struct Pair *segment){
        if(snake->size == snake->capacity){
                int newCapacity = snake->capacity * 2;
                struct Pair* newData = NULL;
                struct Pair* oldData = snake->data;
                
                uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3,
                                  EfiLoaderData,
                                  newCapacity * sizeof(struct Pair),
                                  (void**)&newData
                );
                for(int i = 0; i < snake->size; i++){
                        newData[i] = oldData[i];
                }
                uefi_call_wrapper(SystemTable->BootServices->FreePool, 1, oldData);
                snake->data = newData;
                snake->capacity = newCapacity;
        }
        snake->data[snake->size] = *segment;
        snake->size++;
}

EFI_STATUS random(EFI_RNG_PROTOCOL *rng, struct BoardData *board){
        UINT32 x, y;
        EFI_STATUS status;
        int rangeX = board->width / board->segmentSize;
        int rangeY = board->height / board->segmentSize;
        status = uefi_call_wrapper(rng->GetRNG, 4, rng, NULL, sizeof(UINT32), (UINT8*)&x);
        status = uefi_call_wrapper(rng->GetRNG, 4, rng, NULL, sizeof(UINT32), (UINT8*)&y);
        board->target.x = (int)(x % rangeX) * board->segmentSize;
        board->target.y = (int)(y % rangeY) * board->segmentSize;
        
        return status;
}

void putPixel(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, int x, int y, UINT32 color){
        UINT32* location = (UINT32*)gop->Mode->FrameBufferBase;
        UINT32 pitch = gop->Mode->Info->PixelsPerScanLine;
        location[y * pitch + x] = color;
}

void drawRect(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, int x, int y, int w, int h, UINT32 color){
        for(int i = 0; i < h; i++){
                for(int j = 0; j < w; j++){
                        putPixel(gop, x + j, y + i, color);
                }
        }
}

void drawBoard(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, struct BoardData *board){
        int size = board->segmentSize;
        for(int y = 0; y + size <= board->height; y += size){
                for(int x = 0; x + size <= board->width; x += size){
                        UINT32 currColor;
                        int rowIndex = y / size, colIndex = x / size;
                        if((rowIndex + colIndex) % 2 == 0){
                                currColor = board->color1;
                        }
                        else{
                                currColor = board->color2;
                        }
                        drawRect(gop, x, y, size, size, currColor);
                }
        }
        drawRect(gop, board->target.x, board->target.y, size, size, board->targetColor);
}

bool areOpposite(struct Pair a, struct Pair b){
        return a.x + b.x == 0 && a.y + b.y == 0;
}

int handleKey(EFI_SYSTEM_TABLE *SystemTable, struct Snake *snake){
        EFI_INPUT_KEY key;
        EFI_STATUS status = uefi_call_wrapper(SystemTable->ConIn->ReadKeyStroke, 2, SystemTable->ConIn, &key);
        
        if(EFI_ERROR(status)){
            return OK; 
        }

        if(key.UnicodeChar == 'w'){
                if(snake->segments.size == 1 || !areOpposite(snake->previousDirection, UP)){
                        snake->direction = UP;
                }
        }
        else if(key.UnicodeChar == 'd'){
                if(snake->segments.size == 1 || !areOpposite(snake->previousDirection, RIGHT)){
                        snake->direction = RIGHT;
                }
        }
        else if(key.UnicodeChar == 's'){
                if(snake->segments.size == 1 || !areOpposite(snake->previousDirection, DOWN)){
                        snake->direction = DOWN;
                }
        }
        else if(key.UnicodeChar == 'a'){
                if(snake->segments.size == 1 || !areOpposite(snake->previousDirection, LEFT)){
                        snake->direction = LEFT;
                }
        }
        else if(key.UnicodeChar == 'q'){
                return QUIT;
        }
        return OK;
}

int snakeMove(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, struct Snake *snake, struct BoardData *board, EFI_SYSTEM_TABLE *SystemTable){
        struct Pair* head = &snake->segments.data[0];
        int newX = head->x + snake->direction.x * board->segmentSize;
        int newY = head->y + snake->direction.y * board->segmentSize;


        bool hitWall = (newX >= board->width || newY >= board->height || newX < 0 || newY < 0);
        if(hitWall){
                return DIED;
        }

        bool ateTarget = (newX == board->target.x && newY == board->target.y);
        if(!ateTarget){
                UINT32 color;
                struct Pair *tail = &snake->segments.data[snake->segments.size - 1];
                int rowIndex = tail->y / board->segmentSize, colIndex = tail->x / board->segmentSize;
                if((rowIndex+colIndex) % 2 == 0){
                        color = board->color1;
                }
                else{
                        color = board->color2;
                }
                drawRect(gop, tail->x, tail->y, board->segmentSize, board->segmentSize, color);
        }
        else{
                board->targetAlive = false;
                struct Pair temp = {0,0};
                push_back(SystemTable, &snake->segments, &temp);
                head = &snake->segments.data[0];
        }

        for(int i = snake->segments.size - 1; i > 0; i--){
                snake->segments.data[i] = snake->segments.data[i - 1];
        }

        head->x = newX;
        head->y = newY;

        drawRect(gop, head->x, head->y, board->segmentSize, board->segmentSize, snake->color);
        snake->previousDirection = snake->direction;
        return LIVES;
}

EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable){
        (void)ImageHandle;

        //FOR DEBUGGING
        EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
        EFI_GUID LoadedImageProtocolGUID = EFI_LOADED_IMAGE_PROTOCOL_GUID;
        uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3, ImageHandle, &LoadedImageProtocolGUID, (void **)&loaded_image);
        volatile uint64_t *marker_ptr = (uint64_t *)0x10000;
        volatile uint64_t *image_base_ptr = (uint64_t *)0x10008;
        *image_base_ptr = (uint64_t)loaded_image->ImageBase;
        *marker_ptr = 0xDEADBEEF;
        //Print(L"Program loaded at: 0x%lx\n", (UINT64)loaded_image->ImageBase);
        //FOR DEBUGGING

        uefi_call_wrapper(SystemTable->ConOut->SetAttribute, 2, SystemTable->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK));
        uefi_call_wrapper(SystemTable->ConOut->ClearScreen, 1, SystemTable->ConOut);

        EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
        EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
        EFI_STATUS gopStatus = uefi_call_wrapper(SystemTable->BootServices->LocateProtocol, 3, &gopGuid, NULL, (void**)&gop);

        if(EFI_ERROR(gopStatus)){
                uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u"Couldn't get GOP");
                uefi_call_wrapper(SystemTable->RuntimeServices->ResetSystem, 4, EfiResetShutdown, EFI_SUCCESS, 0, NULL);
                return EFI_SUCCESS;
        }
        uefi_call_wrapper(gop->SetMode, 2, gop, 0);

        EFI_RNG_PROTOCOL *rng;
        EFI_GUID rngGuid = EFI_RNG_PROTOCOL_GUID;
        EFI_STATUS rngStatus = uefi_call_wrapper(SystemTable->BootServices->LocateProtocol, 3, &rngGuid, NULL, (void**)&rng);

        if(EFI_ERROR(rngStatus)){
                uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u"Couldn't get RNG Protocol");
                uefi_call_wrapper(SystemTable->RuntimeServices->ResetSystem, 4, EfiResetShutdown, EFI_SUCCESS, 0, NULL);
                return EFI_SUCCESS;
        }

        int interval = 2500000, segmentSize = 50;        
        int width = (gop->Mode->Info->HorizontalResolution / segmentSize) * segmentSize;
        int height = (gop->Mode->Info->VerticalResolution / segmentSize) * segmentSize;

        struct BoardData board = {
                .width = width, 
                .height = height, 
                .color1 = 0x0090EE90,
                .color2 = 0x0006402B,
                .targetColor = 0x00FF0000,
                .segmentSize = segmentSize,
                .targetAlive = true
        };

        struct Vector segments = {
                .capacity = 1,
                .size = 0
        };

        uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3,
                                  EfiLoaderData,
                                  sizeof(struct Pair),
                                  (void**)&segments.data
        );

        struct Pair start = {100, 100};
        push_back(SystemTable, &segments, &start);


        struct Snake snake = {
                .segments = segments,
                .direction = RIGHT,
                .previousDirection = RIGHT,
                .color = 0x000000FF
        };

        EFI_STATUS randStatus = random(rng, &board);
        if(EFI_ERROR(randStatus)){
                uefi_call_wrapper(SystemTable->RuntimeServices->ResetSystem, 4, EfiResetShutdown, EFI_SUCCESS, 0, NULL);
                return EFI_SUCCESS;
        }

        EFI_EVENT events[2];
        uefi_call_wrapper(SystemTable->BootServices->CreateEvent, 5, EVT_TIMER, 0, NULL, NULL, &events[0]);
        uefi_call_wrapper(SystemTable->BootServices->SetTimer, 3, events[0], TimerPeriodic, interval);
        events[1] = SystemTable->ConIn->WaitForKey;
        drawBoard(gop, &board);

        while(true){
                UINTN index;
                uefi_call_wrapper(SystemTable->BootServices->WaitForEvent, 3, 2, events, &index);
                if(index == 0){
                        int snakeStatus = snakeMove(gop, &snake, &board, SystemTable);
                        if(snakeStatus == DIED){
                                break;
                        }
                        if(!board.targetAlive){
                                random(rng, &board);
                                board.targetAlive = true;
                                drawRect(gop, board.target.x, board.target.y, board.segmentSize, board.segmentSize, board.targetColor);
                        }
                }
                else if(index == 1){
                        int q = handleKey(SystemTable, &snake);
                        if(q == QUIT){
                                break;
                        }
                }
        }



        uefi_call_wrapper(SystemTable->RuntimeServices->ResetSystem, 4, EfiResetShutdown, EFI_SUCCESS, 0, NULL);

        return EFI_SUCCESS;
}