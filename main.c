#include <efi.h>
#include <efilib.h>

#define OK      0
#define QUIT    1

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
};

struct Snake{
    int x;
    int y;
    struct Pair direction;
    UINT32 color;
    int length;
};

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
}

int handleKey(EFI_SYSTEM_TABLE *SystemTable, struct Snake *snake){
        EFI_INPUT_KEY key;
        EFI_STATUS status = uefi_call_wrapper(SystemTable->ConIn->ReadKeyStroke, 2, SystemTable->ConIn, &key);
        
        if(EFI_ERROR(status)){
            return OK; 
        }

        if(key.UnicodeChar == 'w'){
                snake->direction = UP;
        }
        else if(key.UnicodeChar == 'd'){
                snake->direction = RIGHT;
        }
        else if(key.UnicodeChar == 's'){
                snake->direction = DOWN;
        }
        else if(key.UnicodeChar == 'a'){
                snake->direction = LEFT;
        }
        else if(key.UnicodeChar == 'q'){
                return QUIT;
        }
        return OK;
}

void snakeMove(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop, struct Snake *snake, struct BoardData *board){
        UINT32 color;
        int rowIndex = snake->y / board->segmentSize, colIndex = snake->x / board->segmentSize;
        if((rowIndex+colIndex)%2==0){
                color = board->color1;
        }
        else{
                color = board->color2;
        }
        drawRect(gop, snake->x, snake->y, board->segmentSize, board->segmentSize, color);
        snake->x += snake->direction.x * board->segmentSize;
        snake->y += snake->direction.y * board->segmentSize;
        drawRect(gop, snake->x, snake->y, board->segmentSize, board->segmentSize, snake->color);
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
        EFI_STATUS status = uefi_call_wrapper(SystemTable->BootServices->LocateProtocol, 3, &gopGuid, NULL, (void**)&gop);

        if(EFI_ERROR(status)){
                uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u"Couldn't get GOP");
                uefi_call_wrapper(SystemTable->RuntimeServices->ResetSystem, 4, EfiResetShutdown, EFI_SUCCESS, 0, NULL);
                return EFI_SUCCESS;
        }
        uefi_call_wrapper(gop->SetMode, 2, gop, 0);

        int interval = 2500000;        
        int width = gop->Mode->Info->HorizontalResolution;
        int height = gop->Mode->Info->VerticalResolution;

        struct BoardData board = {
                .width = width, 
                .height = height, 
                .color1 = 0x0090EE90,
                .color2 = 0x0006402B,
                .segmentSize = 50,
        };

        struct Snake snake = {
                .x = 100, 
                .y = 100, 
                .direction = RIGHT,
                .color = 0x000000FF, 
                .length = 1
        };
        EFI_EVENT events[2];
        uefi_call_wrapper(SystemTable->BootServices->CreateEvent, 5, EVT_TIMER, 0, NULL, NULL, &events[0]);
        uefi_call_wrapper(SystemTable->BootServices->SetTimer, 3, events[0], TimerPeriodic, interval);
        events[1] = SystemTable->ConIn->WaitForKey;
        drawBoard(gop, &board);

        while(true){
                UINTN index;
                uefi_call_wrapper(SystemTable->BootServices->WaitForEvent, 3, 2, events, &index);
                if(index == 0){
                        snakeMove(gop, &snake, &board);
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
