#include <efi.h>
#include <efilib.h>

#define OK      0
#define QUIT    2
#define DIED    1
#define LIVES   0
#define PLAY    0
#define HALL    1
#define ENTER   u'\r'
#define PADDING_LEFT    10
#define PADDING_UP      5
#define BACKSPACE       0x08
#define ACCELERATION    0.97   
#define LIGHT_GREEN     0x0090EE90
#define DARK_GREEN      0x0006402B
#define RED             0x00FF0000
#define BLUE            0x000000FF
#define SCORE_LENGTH    6 * sizeof(CHAR16)
#define INITIAL_INTERVAL        2500000  
#define RESULTS_PER_PAGE        10
#define SCANCODE_DOWN_ARROW     0x2
#define SCANCODE_UP_ARROW       0x1  
#define SCANCODE_LEFT_ARROW     0x4
#define SCANCODE_RIGHT_ARROW    0x3   
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

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

void free(EFI_SYSTEM_TABLE *SystemTable, struct Vector *snake) {
    uefi_call_wrapper(SystemTable->BootServices->FreePool, 1, snake->data);
    snake->size = 0;
    snake->capacity = 0;
}

EFI_INPUT_KEY getKey(EFI_SYSTEM_TABLE *SystemTable){
        EFI_EVENT events[1];
        EFI_INPUT_KEY key;
        events[0] = SystemTable->ConIn->WaitForKey;
        UINTN index = 0;
        uefi_call_wrapper(SystemTable->BootServices->WaitForEvent, 3, 1, events, &index);
        uefi_call_wrapper(SystemTable->ConIn->ReadKeyStroke, 2, SystemTable->ConIn, &key);
        
        return key;
}

bool isFree(int x, int y, struct Snake *snake){
        for(int i = 0; i < snake->segments.size; i++){
                if(x == snake->segments.data[i].x && y == snake->segments.data[i].y){
                        return false;
                }
        }
        return true;
}

EFI_STATUS getFileProtocol(EFI_SYSTEM_TABLE *SystemTable, EFI_FILE_PROTOCOL** Root){
        EFI_STATUS status;
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fileSystem;
        EFI_GUID guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
        status = uefi_call_wrapper(SystemTable->BootServices->LocateProtocol, 3, &guid, NULL, (void**)&fileSystem);
        if(status != EFI_SUCCESS){
                return status;
        }
        status = uefi_call_wrapper(fileSystem->OpenVolume, 2, fileSystem, Root);
        return status;
}

EFI_STATUS random(EFI_RNG_PROTOCOL *rng, struct BoardData *board, struct Snake *snake){
        UINT32 index;
        EFI_STATUS status;
        int size = board->segmentSize;
        int rangeX = board->width / size;
        int rangeY = board->height / size;
        int range = rangeX * rangeY - snake->segments.size;

        status = uefi_call_wrapper(rng->GetRNG, 4, rng, NULL, sizeof(UINT32), (UINT8*)&index);
        index %= range;

        for(int y = 0; y + size <= board->height; y += size){
                for(int x = 0; x + size <= board->width; x += size){
                        if(isFree(x, y, snake)){
                                if(index == 0){
                                        board->target.x = x;
                                        board->target.y = y;
                                        return status;
                                }
                                index--;
                        }
                }
        }
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

bool checkCollision(struct Snake *snake){
        int headX = snake->segments.data[0].x;
        int headY = snake->segments.data[0].y;
        for(int i = 1; i < snake->segments.size; i++){
                if(headX == snake->segments.data[i].x && headY == snake->segments.data[i].y){
                        return true;
                }
        }
        return false;
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
        bool collision = checkCollision(snake);
        if(collision){
                return DIED;
        }
        return LIVES;
}

int snake(EFI_SYSTEM_TABLE *SystemTable){
        EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
        EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
        EFI_STATUS gopStatus = uefi_call_wrapper(SystemTable->BootServices->LocateProtocol, 3,
                                                        &gopGuid, NULL, (void**)&gop);

        if(EFI_ERROR(gopStatus)){
                uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u"Couldn't get GOP");
                uefi_call_wrapper(SystemTable->RuntimeServices->ResetSystem, 4,
                                        EfiResetShutdown, EFI_SUCCESS, 0, NULL);
                return -1;
        }
        uefi_call_wrapper(gop->SetMode, 2, gop, 0);

        EFI_RNG_PROTOCOL *rng;
        EFI_GUID rngGuid = EFI_RNG_PROTOCOL_GUID;
        EFI_STATUS rngStatus = uefi_call_wrapper(SystemTable->BootServices->LocateProtocol, 3,
                                                        &rngGuid, NULL, (void**)&rng);

        if(EFI_ERROR(rngStatus)){
                uefi_call_wrapper(SystemTable->ConOut->OutputString, 2,
                        SystemTable->ConOut, u"Couldn't get RNG Protocol");
                uefi_call_wrapper(SystemTable->RuntimeServices->ResetSystem, 4,
                        EfiResetShutdown, EFI_SUCCESS, 0, NULL);
                return -1;
        }

        int interval = INITIAL_INTERVAL, segmentSize = 50;   
        int width = (gop->Mode->Info->HorizontalResolution / segmentSize) * segmentSize;
        int height = (gop->Mode->Info->VerticalResolution / segmentSize) * segmentSize;

        struct BoardData board = {
                .width = width, 
                .height = height, 
                .color1 = LIGHT_GREEN,
                .color2 = DARK_GREEN,
                .targetColor = RED,
                .segmentSize = segmentSize,
                .targetAlive = true
        };

        struct Vector segments = {
                .capacity = 1,
                .size = 0
        };

        uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3,
                                EfiLoaderData, sizeof(struct Pair), (void**)&segments.data);

        struct Pair start = {100, 100};
        push_back(SystemTable, &segments, &start);


        struct Snake snake = {
                .segments = segments,
                .direction = RIGHT,
                .previousDirection = RIGHT,
                .color = BLUE
        };

        EFI_STATUS randStatus = random(rng, &board, &snake);
        if(EFI_ERROR(randStatus)){
                uefi_call_wrapper(SystemTable->RuntimeServices->ResetSystem, 4,
                                        EfiResetShutdown, EFI_SUCCESS, 0, NULL);
                return -1;
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
                        if(snakeStatus == DIED || snake.segments.size == (board.width / board.segmentSize) * (board.height / board.segmentSize)){
                                break;
                        }
                        if(!board.targetAlive){
                                random(rng, &board, &snake);
                                board.targetAlive = true;
                                drawRect(gop, board.target.x, board.target.y, board.segmentSize, board.segmentSize, board.targetColor);
                                interval *= ACCELERATION;
                                uefi_call_wrapper(SystemTable->BootServices->SetTimer, 3,
                                                        events[0], TimerPeriodic, interval);
                        }
                }
                else if(index == 1){
                        int q = handleKey(SystemTable, &snake);
                        if(q == QUIT){
                                break;
                        }
                }
        }
        int score = snake.segments.size;
        free(SystemTable, &snake.segments);
        
        return score;
}

int menu(EFI_SYSTEM_TABLE *SystemTable){
        uefi_call_wrapper(SystemTable->ConOut->SetAttribute, 2,
                                SystemTable->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK));
        int currentSelection = PLAY;
        const CHAR16 *options[] = {
                u"      PLAY      ",
                u"  HALL OF FAME  ",
                u"      QUIT      "
        };

        bool change = true;
        while(true){
                if(change){
                        change = false;
                        uefi_call_wrapper(SystemTable->ConOut->ClearScreen, 1, SystemTable->ConOut);
                        for(int i = 0; i < 3; i++){
                                uefi_call_wrapper(SystemTable->ConOut->SetCursorPosition, 3,
                                                        SystemTable->ConOut, PADDING_LEFT, PADDING_UP + i);
                                if(i == currentSelection){
                                        uefi_call_wrapper(SystemTable->ConOut->SetAttribute, 2,
                                                                SystemTable->ConOut,
                                                                EFI_TEXT_ATTR(EFI_WHITE, EFI_BROWN)
                                        );
                                        uefi_call_wrapper(SystemTable->ConOut->OutputString, 2,
                                                                SystemTable->ConOut, options[i]);
                                        uefi_call_wrapper(SystemTable->ConOut->SetAttribute, 2,
                                                                SystemTable->ConOut,
                                                                EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK)
                                        );
                                }
                                else{
                                        uefi_call_wrapper(SystemTable->ConOut->OutputString, 2,
                                                                SystemTable->ConOut, options[i]);
                                }
                        }
                }
                EFI_INPUT_KEY key = getKey(SystemTable);

                if(key.ScanCode == SCANCODE_UP_ARROW){
                        change = (currentSelection != PLAY);
                        if(currentSelection == QUIT){
                                currentSelection = HALL;
                        }
                        else{
                                currentSelection = PLAY;
                        }
                }
                else if(key.ScanCode == SCANCODE_DOWN_ARROW){
                        change = (currentSelection != QUIT);
                        if(currentSelection == PLAY){
                                currentSelection = HALL;
                        }
                        else{
                                currentSelection = QUIT;
                        }
                }
                else if(key.UnicodeChar == ENTER){
                        return currentSelection;
                }
        }
}

void intToString(int x, CHAR16* s){
        if(x == 0){
                s[0] = u'0';
                s[1] = u'\0';
                return;
        }
        int i = 0;
        while(x > 0){
                s[i] = (x % 10) + u'0';
                i++;
                x /= 10;
        }
        
        for(int j = 0; j < i / 2; j++){
                CHAR16 tmp = s[j];
                s[j] = s[i - j - 1];
                s[i - j - 1] = tmp;
        }
        s[i] = u'\0';
}

void saveScore(EFI_SYSTEM_TABLE *SystemTable, int result, CHAR16 name[4]){
        CHAR16 write[7]; 
        CHAR16 read[7]; 
        
        write[0] = name[0];
        write[1] = name[1];
        write[2] = name[2];
        write[3] = (result / 100) % 10 + u'0';
        write[4] = (result / 10) % 10 + u'0';
        write[5] = (result % 10) + u'0';
        write[6] = u'\0';
        bool passed = false;
        UINT64 position = 0;
        
        EFI_FILE_PROTOCOL* root;
        EFI_FILE_PROTOCOL* file;
        getFileProtocol(SystemTable, &root);
        uefi_call_wrapper(root->Open, 5,
                                root,
                                &file,
                                (CHAR16*)u"record.txt",
                                EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                                0
        );

        while(true){
                UINTN size = SCORE_LENGTH;
                uefi_call_wrapper(file->SetPosition, 2, file, position);
                uefi_call_wrapper(file->Read, 3, file, &size, read);

                if(size == 0){
                        uefi_call_wrapper(file->SetPosition, 2, file, position);
                        size = SCORE_LENGTH;
                        uefi_call_wrapper(file->Write, 3, file, &size, write);
                        break;
                }

                if(!passed){
                        int currentScore = (read[3] - u'0') * 100 + (read[4] - u'0') * 10 + (read[5] - u'0');
                        if(result > currentScore){
                                passed = true;
                        }
                }

                if(passed){
                        uefi_call_wrapper(file->SetPosition, 2, file, position);
                        size = SCORE_LENGTH;
                        uefi_call_wrapper(file->Write, 3, file, &size, write);
                        for(int i = 0; i < 7; i++){
                                write[i] = read[i];
                        }
                }
                position += SCORE_LENGTH;
        }
        uefi_call_wrapper(file->Close, 1, file);
        uefi_call_wrapper(root->Close, 1, root);
}

void printResult(EFI_SYSTEM_TABLE *SystemTable, int result){
        uefi_call_wrapper(SystemTable->ConOut->ClearScreen, 1, SystemTable->ConOut);
        uefi_call_wrapper(SystemTable->ConOut->SetAttribute, 2,
                                SystemTable->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK));
        uefi_call_wrapper(SystemTable->ConOut->SetCursorPosition, 3, SystemTable->ConOut, PADDING_LEFT, PADDING_UP); 
        if(result == -1){
                uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u"ERROR OCCURED");
        }
        else{
                CHAR16 score[15];
                intToString(result, score);
                uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u"YOUR SCORE: ");
                uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, score);
        }

        uefi_call_wrapper(SystemTable->ConOut->SetCursorPosition, 3,
                                SystemTable->ConOut, PADDING_LEFT, PADDING_UP + 2); 
        uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u"ENTER YOUR NAME: ");
        int commandLength = PADDING_LEFT + 17;
        CHAR16 name[4] = {u' ', u' ', u' ', u'\0'};
        int counter = 0;
        EFI_INPUT_KEY key;
        while(true){
                key = getKey(SystemTable);
                
                if(key.UnicodeChar == ENTER && counter == 3){
                        break;
                }

                if(key.UnicodeChar == BACKSPACE && counter > 0){
                        counter--;
                        uefi_call_wrapper(SystemTable->ConOut->SetCursorPosition, 3,
                                                SystemTable->ConOut, commandLength + counter, PADDING_UP + 2);
                        uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u" ");
                        name[counter] = u' ';
                }

                else if(key.UnicodeChar >= u'a' && key.UnicodeChar <= u'z' && counter < 3){
                        name[counter] = key.UnicodeChar - 32;
                        CHAR16 str[2] = {name[counter], u'\0'};
                        uefi_call_wrapper(SystemTable->ConOut->SetCursorPosition, 3,
                                                SystemTable->ConOut, commandLength + counter, PADDING_UP + 2);
                        uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, str);
                        counter++;
                }
        }
        saveScore(SystemTable, result, name);
}

UINT64 getFileSize(EFI_SYSTEM_TABLE *SystemTable){
        EFI_FILE_PROTOCOL* root;
        EFI_FILE_PROTOCOL* file;
        getFileProtocol(SystemTable, &root);
        uefi_call_wrapper(root->Open, 5,
                                root,
                                &file,
                                (CHAR16*)u"record.txt",
                                EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                                0
        );
        UINTN size = 0;
        EFI_FILE_INFO *info;

        uefi_call_wrapper(file->GetInfo, 4, file, &gEfiFileInfoGuid, &size, NULL);
        uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, size, (void**)&info);
        uefi_call_wrapper(file->GetInfo, 4, file, &gEfiFileInfoGuid, &size, info);

        UINT64 fileSize = 0;
        fileSize = info->FileSize;
        uefi_call_wrapper(SystemTable->BootServices->FreePool, 1, info);

        uefi_call_wrapper(file->Close, 1, file);
        uefi_call_wrapper(root->Close, 1, root);

        return fileSize;
}

int hallOfFame(EFI_SYSTEM_TABLE *SystemTable, int page, int maximum){
        uefi_call_wrapper(SystemTable->ConOut->ClearScreen, 1, SystemTable->ConOut);
        uefi_call_wrapper(SystemTable->ConOut->SetAttribute, 2,
                                SystemTable->ConOut, EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK));
        EFI_FILE_PROTOCOL* root;
        EFI_FILE_PROTOCOL* file;
        getFileProtocol(SystemTable, &root);
        uefi_call_wrapper(root->Open, 5,
                                root,
                                &file,
                                (CHAR16*)u"record.txt",
                                EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                                0
        );

        CHAR16 buffer[7];
        int counter = RESULTS_PER_PAGE * page + 1;
        uefi_call_wrapper(file->SetPosition, 2, file, page * SCORE_LENGTH * RESULTS_PER_PAGE);
        for(int i = 0; i < RESULTS_PER_PAGE; i++){
                uefi_call_wrapper(SystemTable->ConOut->SetCursorPosition, 3,
                                        SystemTable->ConOut, PADDING_LEFT, PADDING_UP + i); 
                UINTN size = SCORE_LENGTH;
                uefi_call_wrapper(file->Read, 3, file, &size, buffer);
                if(size == 0){
                        break;
                }
                CHAR16 index[15];
                intToString(counter + i, index);
                CHAR16 name[] = {buffer[0], buffer[1], buffer[2], u'\0'};

                uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, index);
                uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u". ");
                uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, name);
                uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u" - ");
                bool zeros = true;
                for(int j = 3; j < 6; j++){
                        if(buffer[j] != u'0' || !zeros){
                                zeros = false;
                                CHAR16 digit[2] = {buffer[j], u'\0'};
                                uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, digit);
                        }
                }
        }
        CHAR16 curr[15];
        if(maximum >= 0){
                intToString(page + 1, curr);
        }
        else{
                intToString(0, curr);
        }
        CHAR16 maxPage[15];
        intToString(maximum + 1, maxPage);
        uefi_call_wrapper(SystemTable->ConOut->SetCursorPosition, 3,
                                SystemTable->ConOut, PADDING_LEFT, PADDING_UP + RESULTS_PER_PAGE + 1); 
        uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u"<- (");
        uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, curr);
        uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u"/");
        uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, maxPage);
        uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u") ->");

        uefi_call_wrapper(SystemTable->ConOut->SetCursorPosition, 3,
                                SystemTable->ConOut, PADDING_LEFT, PADDING_UP + RESULTS_PER_PAGE + 2); 
        uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, u"PRESS Q TO LEAVE");

        uefi_call_wrapper(file->Close, 1, file);
        uefi_call_wrapper(root->Close, 1, root);



        while(true){
                EFI_INPUT_KEY key = getKey(SystemTable);
                if(key.ScanCode == SCANCODE_LEFT_ARROW && page - 1 >= 0){
                        return page - 1;
                }
                if(key.ScanCode == SCANCODE_RIGHT_ARROW && page + 1 <= maximum){
                        return page + 1;
                }
                if(key.UnicodeChar == u'q'){
                        return -1;
                }
        }

}

void hall(EFI_SYSTEM_TABLE *SystemTable){
        int page = 0, maximum;
        UINT64 fileSize = getFileSize(SystemTable);
        maximum = fileSize / (RESULTS_PER_PAGE * SCORE_LENGTH);
        if(fileSize % (RESULTS_PER_PAGE * SCORE_LENGTH) == 0){
                maximum--;
        }

        while(true){
                page = hallOfFame(SystemTable, page, maximum);
                if(page == -1){
                        break;
                }
        }
}

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable){
        (void)ImageHandle;

        //FOR DEBUGGING
        EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
        EFI_GUID LoadedImageProtocolGUID = EFI_LOADED_IMAGE_PROTOCOL_GUID;
        uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
                                ImageHandle, &LoadedImageProtocolGUID, (void **)&loaded_image);
        volatile uint64_t *marker_ptr = (uint64_t *)0x10000;
        volatile uint64_t *image_base_ptr = (uint64_t *)0x10008;
        *image_base_ptr = (uint64_t)loaded_image->ImageBase;
        *marker_ptr = 0xDEADBEEF;
        //FOR DEBUGGING

        while(true){
                int choice = menu(SystemTable);

                if(choice == QUIT){
                        break;
                }

                if(choice == PLAY){
                        int result = snake(SystemTable);
                        printResult(SystemTable, result);
                }

                else if(choice == HALL){
                        hall(SystemTable);
                }
        }

        uefi_call_wrapper(SystemTable->RuntimeServices->ResetSystem, 4, EfiResetShutdown, EFI_SUCCESS, 0, NULL);

        return EFI_SUCCESS;
}