#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unordered_map>
#include <ctime>
#include <emscripten/emscripten.h>

#define CONTAINER 2
#define FREE_SPACE 0
#define WALL 3
#define AIM_CELL 4
#define AIM_OCCUPIED_PLAYER 5
#define AIM_OCCUPIED_CONTAINER 6
#define PLAYER_SPOT 7
#define MATRIX_SIZE 8
#define MAX_PRIME_NUMBER 529
#define Hash std::unordered_map<uint64_t, bool>
#define LHash std::unordered_map<uint64_t, int8_t>

enum {
    UP,
    DOWN,
    LEFT,
    RIGHT
};

#define INITIAL_CAPACITY (1<<21)

clock_t Start;

struct PQueue_t {
  int16_t *heap;
  void **indexes;
  int32_t size;
};

typedef struct PQueue_t *PQueue;

PQueue pq_New(void) {
  PQueue queue = (struct PQueue_t*)malloc(sizeof(struct PQueue_t));
  queue->heap = (int16_t*)malloc(sizeof(int16_t) * INITIAL_CAPACITY);
  queue->indexes = (void**)malloc(sizeof(void*) * INITIAL_CAPACITY);
  queue->size = 0;
  return queue;
}

void fswap(int16_t *array, int32_t a, int32_t b) {
  int16_t aux = array[a];
  array[a] = array[b];
  array[b] = aux;
}

void swap(void **array, int32_t a, int32_t b) {
  void *aux = array[a];
  array[a] = array[b];
  array[b] = aux;
}

void pushElement(PQueue queue, int32_t index) {
    if(!index) {
        return ;
    }
    int32_t parent = (index - 1) >> 1;
    if(queue->heap[index] < queue->heap[parent]) {
        fswap(queue->heap, index, parent);
        swap(queue->indexes, index, parent);
        pushElement(queue, parent);
    }
}

void pq_Push(PQueue queue, uint32_t value, void *index) {
  queue->heap[queue->size] = value;
  queue->indexes[queue->size] = index;
  pushElement(queue, queue->size);
  queue->size++;
}

void pq_DeleteHead(PQueue queue) {
  int16_t *heap = queue->heap;
  void **indexes = queue->indexes;
  int32_t heapSize = queue->size;
  int32_t index = 0;
  fswap(heap, 0, heapSize - 1);
  swap(indexes, 0, heapSize - 1);
  queue->size--;
  heapSize--;
  while(((index << 1) + 1 < heapSize && heap[index] > heap[(index << 1) + 1]) ||
        ((index << 1) + 2 < heapSize && heap[index] > heap[(index << 1) + 2])) {
    if((index << 1) + 2 >= heapSize) {
      fswap(heap, index, (index << 1) + 1);
      swap(indexes, index, (index << 1) + 1);
      index <<= 1;
      index++;
    }
    else if(heap[(index << 1) + 2] >= heap[(index << 1) + 1]) {
      fswap(heap, index, (index << 1) + 1);
      swap(indexes, index, (index << 1) + 1);
      index <<= 1;
      index++;
    }
    else {
      fswap(heap, index, (index << 1) + 2);
      swap(indexes, index, (index << 1) + 2);
      index <<= 1;
      index += 2;
    }
  }
}

void pq_Delete(PQueue queue) {
    queue->size = 0;
}

void pq_DeleteFromMemory(PQueue queue) {
    free(queue->heap);
    free(queue->indexes);
    free(queue);
}

void *pq_GetMinValueIndex(PQueue queue) {
  if(!queue->size)
    return NULL;
  return queue->indexes[0];
}

int16_t pq_GetMinValue(PQueue queue) {
  if(!queue->size)
    return -1;
  return queue->heap[0];
}

typedef struct Player_t *Player;
typedef struct Map_t *Map;
struct Container_t;
typedef struct Container_t *Container;
typedef struct AimCell_t *AimCell;
struct Stack_t;

struct Map_t {
    unsigned char **map;
    uint8_t sizeX;
    uint8_t sizeY;
    Player player;
    Hash *hashMap;
    uint32_t memIndex;
    uint8_t *forbiddenPlaces;
    uint64_t aimCellsHash;
    uint64_t containerCellsHashes;
    int16_t precalcLog[530];
    uint64_t freeObjects;
    uint64_t walls;
    int8_t precalcDistances[1<<7];
    uint64_t containersIDs[4];
    uint64_t containersChecks[4];
    uint64_t aimCellsChecks[4];
    uint64_t aimCellsIDs[4];
    int8_t objectsCount;
    int8_t pairs[2][1<<7];
    uint8_t distMatrix[1<<6][1<<6];
    struct Stack_t *prealocatedMemoryStack;
    uint8_t *response;
    uint32_t responseIndex;
};

uint8_t moveBox(Map self, uint8_t y, uint8_t x, uint8_t dy, uint8_t dx, uint8_t lastCell);
uint8_t pl_Move(Player player, uint8_t direction);
static inline void addContainerById(Map self, uint8_t element, uint8_t ID);
static inline uint8_t getIdByContainer(Map self, uint8_t element);
static inline uint8_t getContainerById(Map self, uint8_t ID);
static inline void addAimCellById(Map self, uint8_t element, uint8_t ID);
static inline uint8_t getIdByAimCell(Map self, uint8_t element);
static inline uint8_t getAimCellById(Map self, uint8_t ID);
static inline void addContainerAtID(Map self, uint8_t element, uint8_t ID);

struct Player_t {
    Map playGround;
    uint8_t y;
    uint8_t x;
};

struct Container_t {
    uint8_t value;
};

struct AimCell_t {
    uint8_t value;
};

struct Stack_t {
    uint64_t containers;
    uint32_t stackIndex;
    uint64_t containerIDs[4];
    uint64_t containerChecker[4];
};

uint64_t mm_HashValue(Map self) {
    return self->containerCellsHashes | ((uint64_t)self->player->y << 56LL) | ((uint64_t)self->player->x << 59LL);
}

struct Stack_t *mm_GetStack(Map self, int32_t ID) {
    struct Stack_t *state = &self->prealocatedMemoryStack[self->memIndex++];
    state->containers = self->containerCellsHashes | ((uint64_t)(self->player->y) << 56LL) | ((uint64_t)(self->player->x) << 59LL);
    state->stackIndex = ID;
    memcpy(state->containerIDs, self->containersIDs, sizeof(uint64_t) * 4);
    memcpy(state->containerChecker, self->containersChecks, sizeof(uint64_t) * 4);
    return state;
}

void mm_SetState(Map self, struct Stack_t *state) {
    self->player->x = (int8_t)(((7LL << 59LL) & state->containers) >> 59LL);
    self->player->y = (int8_t)(((7LL << 56LL) & state->containers) >> 56LL);
    self->containerCellsHashes = state->containers & ((1LL << 56LL) - 1LL);
    memcpy(self->containersIDs, state->containerIDs, sizeof(uint64_t) * 4);
    memcpy(self->containersChecks, state->containerChecker, sizeof(uint64_t) * 4);
}

void findAgent(Map self) {
    for(uint8_t i = 0; i < self->sizeY; i++) {
        for(uint8_t j = 0; j < self->sizeX; j++) {
            if(self->map[i][j] == PLAYER_SPOT || self->map[i][j] == AIM_OCCUPIED_PLAYER) {
                self->player->y = i;
                self->player->x = j;
                return ;
            }
        }
    }
    return ;
}

void transformMap(Map self, unsigned char **map) {
    for(uint8_t i = 0; i < self->sizeY; i++) {
        for(uint8_t j = 0; j < self->sizeX; j++) {
            switch(map[i][j]) {
                case ' ':
                    self->map[i][j] = FREE_SPACE;
                    break;
                case '@':
                    self->player->y = i;
                    self->player->x = j;
                    self->map[i][j] = FREE_SPACE;
                    break;
                case '+':
                    self->player->y = i;
                    self->player->x = j;
                    self->map[i][j] = AIM_CELL;
                    break;
                case '.':
                    self->map[i][j] = AIM_CELL;
                    break;
                case '#':
                    self->map[i][j] = WALL;
                    break;
                case '$':
                    self->map[i][j] = CONTAINER;
                    break;
                case '*':
                    self->map[i][j] = AIM_OCCUPIED_CONTAINER;
                    break;
                default:
                    self->map[i][j] = FREE_SPACE;
                    break;
            }
        }
    }
}

int8_t isCellOcupied(Map self, int8_t dy, int8_t dx) {
    return self->walls & (1LL << (uint64_t)((self->player->y + dy) * MATRIX_SIZE + (self->player->x + dx))) ||
           ((1LL << (uint64_t)((self->player->y + dy) * MATRIX_SIZE + (self->player->x + dx))) & self->containerCellsHashes);
}

uint8_t pl_Move(Player player, uint8_t direction) {
    Map self = player->playGround;
    if(direction == UP) {
        if(moveBox(player->playGround, player->y - 1, player->x, -1, 0, FREE_SPACE)) {
            player->y--;
            return 2;
        }
        if(!isCellOcupied(self, -1, 0)) {
            player->y--;
            return 1;
        }
    }
    if(direction == DOWN) {
        if(moveBox(player->playGround, player->y + 1, player->x, 1, 0, FREE_SPACE)) {
            player->y++;
            return 2;
        }
        if(!isCellOcupied(self, 1, 0)) {
            player->y++;
            return 1;
        }
    }
    if(direction == LEFT) {
        if(moveBox(player->playGround, player->y, player->x - 1, 0, -1, FREE_SPACE)) {
            player->x--;
            return 2;
        }
        if(!isCellOcupied(self, 0, -1)) {
            player->x--;
            return 1;
        }
    }
    if(direction == RIGHT) {
        if(moveBox(player->playGround, player->y, player->x + 1, 0, 1, FREE_SPACE)) {
            player->x++;
            return 2;
        }
        if(!isCellOcupied(self, 0, 1)) {
            player->x++;
            return 1;
        }
    }
    return 0;
}

uint8_t mm_Goal(Map self) {
    return self->aimCellsHash == self->containerCellsHashes;
}

void mm_Show(Map self) {
    for(uint8_t i = 0; i < self->sizeY; i++) {
        for(uint8_t j = 0; j < self->sizeX; j++) {
            uint64_t position = i * MATRIX_SIZE + j;
            if(self->aimCellsHash & (1LL << position) && self->player->x == j && self->player->y == i) {
                printf("%d", AIM_OCCUPIED_PLAYER);
            }
            else if(self->player->x == j && self->player->y == i) {
                printf("%d", PLAYER_SPOT);
            }
            else if(self->containerCellsHashes & (1LL << position) && self->aimCellsHash & (1LL << position)) {
                printf("%d", AIM_OCCUPIED_CONTAINER);
            }
            else if(self->containerCellsHashes & (1LL << position)) {
                printf("%d", CONTAINER);
            }
            else if(self->aimCellsHash & (1LL << position)) {
                printf("%d", AIM_CELL);
            }
            else {
                printf("%d", self->map[i][j]);
            }
        }
        printf("\n");
    }
    printf("Is Done? %d\n", mm_Goal(self));
    printf("DONE!\n\n");
}

uint8_t moveBox(Map self, uint8_t y, uint8_t x, uint8_t dy, uint8_t dx, uint8_t lastCell) {
    uint64_t arrayPosition = y * MATRIX_SIZE + x;
    uint64_t containerHash = self->containerCellsHashes;
    if(!((1LL << arrayPosition) & containerHash)) {
        return 0;
    }
    uint8_t nextX = x + dx;
    uint8_t nextY = y + dy;
    uint64_t nextPosition = nextY * MATRIX_SIZE + nextX;
    uint64_t aimHash = self->aimCellsHash;
    if((!(self->walls & (1LL << (uint64_t)(nextPosition))) || ((1LL << nextPosition) & aimHash)) && !((1LL << nextPosition) & containerHash)) {
        self->containerCellsHashes ^= (1LL << arrayPosition);
        self->containerCellsHashes |= (1LL << nextPosition);
        uint8_t elementID = getIdByContainer(self, arrayPosition);
        addContainerById(self, nextPosition, elementID);
        return 1;
    }
    return 0;
}

uint8_t mm_isHashUsed(Map self) {
    uint64_t key = mm_HashValue(self);
    if(self->hashMap->find(key) != self->hashMap->end()) {
        return 1;
    }
    return 0;
}

uint8_t mm_CanMove(Map self, int8_t y, int8_t x) {
    uint64_t nextPosition = (self->player->y + y) * MATRIX_SIZE + (self->player->x + x);
    uint64_t nextPositionContainer = (self->player->y + (y << 1)) * MATRIX_SIZE + (self->player->x + (x << 1));
    if(!self->forbiddenPlaces[nextPositionContainer] && ((1LL << nextPosition) & self->containerCellsHashes) && !((1LL << nextPositionContainer) & self->containerCellsHashes) &&
       !(self->walls & (1LL << (uint64_t)(nextPositionContainer)))) {
        return 1;
    }
    if(!isCellOcupied(self, y, x)) {
        return 1;
    }
    return 0;
}

int32_t mm_GetHeuristicsBoxesDist(Map self, int8_t playerY, int8_t playerX) {
    uint64_t firstHash = self->containerCellsHashes;
    uint64_t secondHash = self->aimCellsHash;
    int32_t heuristics = (1<<26) - 1;
    uint8_t index = 0;
    while(firstHash) {
        int8_t x = index & 7;
        int8_t y = index >> 3;
        uint64_t secondHashCopy = secondHash;
        uint8_t aimCellIndex = 0;
        int32_t aimDistance = 0;
        while(secondHashCopy) {
            int8_t xx = aimCellIndex & 7;
            int8_t yy = aimCellIndex >> 3;
            int32_t dist = abs(x - xx) + abs(y - yy);
            aimDistance += dist;
            secondHashCopy >>= 1;
            aimCellIndex++;
        }
        int32_t dist = abs(playerX - x) + abs(playerY - y);
        if(heuristics > dist + aimDistance) {
            heuristics = dist + aimDistance;
        }
        index++;
        firstHash >>= 1;
    }
    return heuristics;
}

int32_t mm_GetHeuristicsMinBox(Map self, int8_t playerY, int8_t playerX) {
    uint64_t firstHash = self->containerCellsHashes;
    int32_t heuristics = (1<<26) - 1;
    uint8_t index = 0;
    int16_t *prec = self->precalcLog;
    while(firstHash) {
        index = prec[((firstHash | (firstHash - 1)) ^ firstHash) % MAX_PRIME_NUMBER];
        firstHash &= (firstHash - 1);
        int8_t x = index & 7;
        int8_t y = index >> 3;
        int32_t minDist = abs(playerX - x) + abs(playerY - y);
        if(heuristics > minDist) {
            heuristics = minDist;
        }
    }
    return heuristics;
}

int32_t mm_ZeroHeuristics(Map self, int8_t playerY, int8_t playerX) {
    return 0;
}

int32_t mm_GetHeuristicsMinAllBox(Map self, int8_t playerY, int8_t playerX) {
    uint64_t firstHash = self->containerCellsHashes;
    uint64_t secondHash = self->aimCellsHash;
    int32_t heuristics = 0;
    int8_t index = 0;
    int16_t *prec = self->precalcLog;
    while(firstHash) {
        index = prec[((firstHash | (firstHash - 1)) ^ firstHash) % MAX_PRIME_NUMBER];
        firstHash &= (firstHash - 1);
        int8_t x = index & 7;
        int8_t y = index >> 3;
        uint64_t secondHashCopy = secondHash;
        uint8_t aimCellIndex = 0;
        int32_t aimDistance = (1<<26) - 1;
        while(secondHashCopy) {
            aimCellIndex = prec[((secondHashCopy | (secondHashCopy - 1)) ^ secondHashCopy) % MAX_PRIME_NUMBER];
            secondHashCopy &= (secondHashCopy - 1);
            int8_t xx = aimCellIndex & 7;
            int8_t yy = aimCellIndex >> 3;
            int32_t dist = abs(x - xx) + abs(y - yy);
            if(aimDistance > dist) {
                aimDistance = dist;
            }
        }
        heuristics += aimDistance;
    }
    return heuristics;
}

int32_t mm_GetHeuristicsMinAllBoxUpdated(Map self, int8_t playerY, int8_t playerX) {
    uint64_t firstHash = self->containerCellsHashes;
    int32_t heuristics = 0;
    int8_t index = 0;
    int16_t *prec = self->precalcLog;
    int8_t *precalcDistances = self->precalcDistances;
    while(firstHash) {
        index = prec[((firstHash | (firstHash - 1)) ^ firstHash) % MAX_PRIME_NUMBER];
        firstHash &= (firstHash - 1);
        heuristics += (int32_t)precalcDistances[index];
    }
    return heuristics;
}

uint8_t isStuck(Map self) {
    uint64_t firstHash = self->containerCellsHashes;
    uint64_t secondHash = self->aimCellsHash;
    int8_t dx[] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
    int8_t dy[] = {-1, -1, 0, 1, 1, 1, 0, -1, -1};
    uint8_t index = 0;
    int16_t *prec = self->precalcLog;
    int8_t *precalcDistances = self->precalcDistances;
    while(firstHash) {
        index = prec[((firstHash | (firstHash - 1)) ^ firstHash) % MAX_PRIME_NUMBER];
        firstHash &= (firstHash - 1);
        if(precalcDistances[index] == 127) {
            return 1;
        }
        if(!((secondHash >> (uint64_t)index) & 1)) {
            for(int8_t i = 0; i < 8; i += 2) {
                int8_t checker = 0;
                for(int8_t j = 0; j < 3; j++) {
                    int8_t y = (index >> 3) + dy[i + j];
                    int8_t x = (index & 7) + dx[i + j];
                    if(!((self->walls & (1ULL << (uint64_t)(y * MATRIX_SIZE + x))) || (1LL << (uint64_t)(y * MATRIX_SIZE + x) & self->containerCellsHashes))) {
                        checker = 1;
                        break;
                    }
                }
                if(!checker) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

void mm_SearchDataAstar(Map self, int32_t (*heuristicFunction)(Map, int8_t, int8_t), size_t maxNumberOfSteps, uint8_t *path, int32_t *parents, uint8_t *actions, uint8_t *isDone);

void mm_SearchDataAstar(Map self, int32_t (*heuristicFunction)(Map, int8_t, int8_t),
                        size_t maxNumberOfSteps, uint8_t *path, int32_t *parents, uint8_t *actions, PQueue queue, uint8_t *isDone) {
    self->hashMap->clear();
    uint8_t directions[] = {UP, DOWN, LEFT, RIGHT};
    int8_t dx[] = {0, 0, -1, 1};
    int8_t dy[] = {-1, 1, 0, 0};
    uint16_t actionsSize = 0;
    size_t head = 0;
    size_t tail = 1;
    struct Stack_t *stackElement = mm_GetStack(self, 0);
    struct Stack_t *firstState = mm_GetStack(self, 0);
    pq_Push(queue, 0, stackElement);
    parents[stackElement->stackIndex] = -1;
    (*self->hashMap)[mm_HashValue(self)] = 1;
    Player player = self->player;
    while(tail < maxNumberOfSteps) {
        struct Stack_t *state = (struct Stack_t *)pq_GetMinValueIndex(queue);
        if(!state || isStuck(self)) {
            //printf("Solutions not reached!\n");
            pq_Delete(queue);
            return ;
        }
        mm_SetState(self, state);
        if(mm_Goal(self)) {
            int32_t lastIndex = state->stackIndex;
            mm_SetState(self, firstState);
            while(parents[lastIndex] != -1) {
                actions[actionsSize++] = path[lastIndex];
                lastIndex = parents[lastIndex];
            }
            for(int32_t i = actionsSize - 1; i >= 0; i--) {
                if(actions[i] == UP + 5) {
                    //printf("U");
                    self->response[self->responseIndex++] = 'U';
                }
                if(actions[i] == DOWN + 5) {
                   // printf("D");
                    self->response[self->responseIndex++] = 'D';
                }
                if(actions[i] == LEFT + 5) {
                   // printf("L");
                    self->response[self->responseIndex++] = 'L';
                }
                if(actions[i] == RIGHT + 5) {
                   // printf("R");
                    self->response[self->responseIndex++] = 'R';
                }
                if(actions[i] == UP) {
                   // printf("u");
                    self->response[self->responseIndex++] = 'u';
                }
                if(actions[i] == DOWN) {
                   // printf("d");
                    self->response[self->responseIndex++] = 'd';
                }
                if(actions[i] == LEFT) {
                   // printf("l");
                    self->response[self->responseIndex++] = 'l';
                }
                if(actions[i] == RIGHT) {
                   // printf("r");
                    self->response[self->responseIndex++] = 'r';
                }
            }
            pq_DeleteFromMemory(queue);
           // exit(0);
            *isDone = 1;
            return ;
        }
        pq_DeleteHead(queue);
        for(int8_t i = 0; i < 4; i++) {
            uint8_t canDo = mm_CanMove(self, dy[i], dx[i]);
            if(canDo) {
                uint8_t hasPushed = pl_Move(self->player, directions[i]);
                if(hasPushed && !mm_isHashUsed(self) && !isStuck(self)) {
                    int32_t heuristicNumber = isStuck(self) * 100;
                    struct Stack_t *childState = mm_GetStack(self, tail);
                    (*self->hashMap)[childState->containers] = 1;
                    pq_Push(queue, heuristicFunction(self, player->y, player->x) + heuristicNumber, childState);
                    parents[childState->stackIndex] = state->stackIndex;
                    if(hasPushed == 2) {
                        path[childState->stackIndex] = directions[i] + 5;
                    }
                    else {
                        path[childState->stackIndex] = directions[i];
                    }
                    tail++;
                }
                mm_SetState(self, state);
            }
        }
        head++;
    }
    pq_Delete(queue);
}

void getContainers(Map self) {
    for(uint8_t i = 0; i < self->sizeY; i++) {
        for(uint8_t j = 0; j < self->sizeX; j++) {
            if(self->map[i][j] == CONTAINER ||
               self->map[i][j] == AIM_OCCUPIED_CONTAINER) {
                self->containerCellsHashes |= (1LL << (uint64_t)(i * MATRIX_SIZE + j));
            }
        }
    }
}

void getAimCells(Map self) {
    for(uint8_t i = 0; i < self->sizeY; i++) {
        for(uint8_t j = 0; j < self->sizeX; j++) {
            if(self->map[i][j] == AIM_OCCUPIED_CONTAINER ||
               self->map[i][j] == AIM_OCCUPIED_PLAYER ||
               self->map[i][j] == AIM_CELL) {
                self->aimCellsHash |= (1LL << (uint64_t)(i * MATRIX_SIZE + j));
            }
        }
    }
}

void getWall(Map self) {
    for(int8_t i = 0; i < self->sizeY; i++) {
        for(int8_t j = 0; j < self->sizeX; j++) {
            if(self->map[i][j] == WALL) {
                self->walls |= (1ULL << (uint64_t)(i * MATRIX_SIZE + j));
            }
        }
    }
}

void createLockers(Map self) {
    uint64_t secondHash = self->aimCellsHash;
    int8_t dx[] = {0, 1, 1, 1, 0, -1, -1, -1, 0};
    int8_t dy[] = {-1, -1, 0, 1, 1, 1, 0, -1, -1};
    for(int8_t index = 8; index < 56; index++) {
        if(!(secondHash & (1LL << (int64_t)index)) && !(self->walls & (1LL << (uint64_t)(index)))) {
            for(int8_t i = 0; i < 8; i += 2) {
                int8_t checker = 0;
                for(int8_t j = 0; j < 3; j++) {
                    int8_t y = (index >> 3) + dy[i + j];
                    int8_t x = (index & 7) + dx[i + j];
                    if(!(self->map[y][x] == WALL)) {
                        checker = 1;
                        break;
                    }
                }
                if(!checker) {
                    self->forbiddenPlaces[index] = 1;
                }
            }
        }
    }
}

void clearMap(Map self) {
    for(uint8_t i = 0; i < self->sizeY; i++) {
        for(uint8_t j = 0; j < self->sizeX; j++) {
            if(!(self->map[i][j] == FREE_SPACE || self->map[i][j] == WALL)) {
                self->map[i][j] = FREE_SPACE;
            }
        }
    }
}

void createPrecald(Map self) {
    for(uint64_t i = 0; i < 65; i++) {
        self->precalcLog[((1ULL << i) - 1ULL) % MAX_PRIME_NUMBER] = i;
        self->precalcDistances[i] = (1 << 7) - 1;
    }
}

uint8_t distance(int8_t first, int8_t second) {
    int8_t x = first & 7;
    int8_t y = first >> 3;
    int8_t xx = second & 7;
    int8_t yy = second >> 3;
    return abs(x - xx) + abs(y - yy);
}

uint8_t bfsDistance(Map self, int8_t first, int8_t second) {
    int8_t x = first & 7;
    int8_t y = first >> 3;
    int8_t xx = second & 7;
    int8_t yy = second >> 3;
    if(first == second)
        return 0;
    int8_t xC[1<<6];
    int8_t yC[1<<6];
    bool hsh[1<<7] = {0};
    int8_t dx[] = {-1, 0, 1, 0};
    int8_t dy[] = {0, 1, 0, -1};
    int8_t tail = 1;
    int8_t head = 0;
    xC[0] = x;
    yC[0] = y;
    int8_t distances[1<<6][1<<6];
    for(int32_t i = 0; i < 1<<6; i++) {
        for(int32_t j = 0; j < 1<<6; j++) {
            distances[i][j] = 0;
        }
    }
    while(tail != head) {
        for(int8_t i = 0; i < 4; i++) {
            int8_t cx = dx[i] + xC[head];
            int8_t cy = dy[i] + yC[head];
            if(!hsh[cy * MATRIX_SIZE + cx] && self->freeObjects & (1LL << (uint64_t)(cy * MATRIX_SIZE + cx))
                && !(self->containerCellsHashes & (1LL << (uint64_t)(cy * MATRIX_SIZE + cx)))) {
                distances[cy][cx] = distances[yC[head]][xC[head]] + 1;
                hsh[cy * MATRIX_SIZE + cx] = 1;
                xC[tail] = cx;
                yC[tail] = cy;
                tail++;
            }
            if(distances[yy][xx]) {
                return distances[yy][xx];
            }
        }
        head++;
    }
    return 100;
}

void createHeuristicDistance(Map self) {
    uint64_t containers = self->containerCellsHashes;
    uint8_t index = 0;
    int16_t *prec = self->precalcLog;
    int8_t contIndex = 0;
    while(containers) {
        index = prec[((containers | (containers - 1)) ^ containers) % MAX_PRIME_NUMBER];
        addContainerById(self, index, contIndex);
        contIndex++;
        containers &= (containers - 1);
    }
    index = 0;
    contIndex = 0;
    uint64_t aimCells = self->aimCellsHash;
    while(aimCells) {
        index = prec[((aimCells | (aimCells - 1)) ^ aimCells) % MAX_PRIME_NUMBER];
        addAimCellById(self, index, contIndex);
        contIndex++;
        aimCells &= (aimCells - 1);
    }
    self->objectsCount = contIndex;
    for(int8_t i = 0; i < self->objectsCount; i++) {
        self->pairs[0][i] = i;
        self->pairs[1][i] = i;
    }
}

int32_t mm_GetHeuristicsPair(Map self, int8_t playerY, int8_t playerX) {
    int32_t heuristics = 0;
    for(int8_t i = 0; i < self->objectsCount; i++) {
        int8_t container = getContainerById(self, self->pairs[0][i]);
        int8_t aimCell = getAimCellById(self, self->pairs[1][i]);
        int8_t x = container & 7;
        int8_t y = container >> 3;
        int8_t xx = aimCell & 7;
        int8_t yy = aimCell >> 3;
        int32_t dist = abs(x - xx) + abs(y - yy);
        heuristics += dist;
    }
    return heuristics;
}

void bfs(Map self, int8_t y, int8_t x) {
    int8_t dx[] = {-1, 0, 1, 0};
    int8_t dy[] = {0, 1, 0, -1};
    int8_t head = 0;
    int8_t tail = 1;
    int8_t precX[1<<8];
    int8_t precY[1<<8];
    int8_t distanceMatrix[10][10] = {{0}};
    for(int8_t i = 0; i < 10; i++)
        for(int8_t j = 0; j < 10; j++)
            distanceMatrix[i][j] = 127;
    distanceMatrix[y][x] = 1;
    self->precalcDistances[(uint64_t)(y * MATRIX_SIZE + x)] = 1;
    precX[0] = x;
    precY[0] = y;
    while(head != tail) {
        for(int8_t i = 0; i < 4; i++) {
            int8_t cx = dx[i] + precX[head];
            int8_t cy = dy[i] + precY[head];
            int8_t lastPosX = (dx[i] << 1) + precX[head];
            int8_t lastPosY = (dy[i] << 1) + precY[head];
            if(self->freeObjects & (1LL << (uint64_t)(cy * MATRIX_SIZE + cx)) && distanceMatrix[cy][cx] == 127 && self->map[lastPosY][lastPosX] != WALL) {
                distanceMatrix[cy][cx] = distanceMatrix[precY[head]][precX[head]] + 1;
                precX[tail] = cx;
                precY[tail] = cy;
                tail++;
            }
        }
        head++;
    }
    for(int8_t i = 5; i < 56; i++) {
        if(self->freeObjects & (1LL << (uint64_t)i)) {
            int8_t cx = i & 7;
            int8_t cy = i >> 3;
            if(self->precalcDistances[i] > distanceMatrix[cy][cx]) {
                self->precalcDistances[i] = distanceMatrix[cy][cx];
            }
        }
    }
}

void precalculateClosestBox(Map self) {
    uint64_t goals = self->aimCellsHash;
    int8_t index = 0;
    int16_t *prec = self->precalcLog;
    while(goals) {
        index = prec[((goals | (goals - 1)) ^ goals) % MAX_PRIME_NUMBER];
        int8_t dx = index & 7;
        int8_t dy = index >> 3;
        bfs(self, dy, dx);
        goals &= (goals - 1);
    }
}

void findInteriorPoints(Map self) {
    int8_t dx[] = {-1, 0, 1, 0};
    int8_t dy[] = {0, 1, 0, -1};
    int8_t head = 0;
    int8_t tail = 1;
    int8_t precX[1<<8];
    int8_t precY[1<<8];
    int8_t checked[1<<8] = {0};
    precX[0] = self->player->x;
    precY[0] = self->player->y;
    while(head != tail) {
        for(int8_t i = 0; i < 4; i++) {
            int8_t cx = precX[head] + dx[i];
            int8_t cy = precY[head] + dy[i];
            if(!checked[cy * MATRIX_SIZE + cx] && self->map[cy][cx] != WALL) {
                checked[cy * MATRIX_SIZE + cx] = 1;
                self->freeObjects |= (1ULL << (uint64_t)(cy * MATRIX_SIZE + cx));
                precX[tail] = cx;
                precY[tail] = cy;
                tail++;
            }
        }
        head++;
    }
}

static inline void addContainerById(Map self, uint8_t element, uint8_t ID) {
    self->containersIDs[ID / 10] &= ~((uint64_t)63 << (uint64_t)(ID % 10 * 6));
    self->containersIDs[ID / 10] |= ((uint64_t)element << (uint64_t)(ID % 10 * 6));
    self->containersChecks[element >> 4] &= ~((uint64_t)15 << (uint64_t)((element & 15) << 2));
    self->containersChecks[element >> 4] |= ((uint64_t)ID << (uint64_t)((element & 15) << 2));
}

static inline uint8_t getIdByContainer(Map self, uint8_t element) {
    return (uint8_t)((self->containersChecks[element >> 4] >> ((element & 15) << 2)) & 15ULL);
}

static inline uint8_t getContainerById(Map self, uint8_t ID) {
    return (self->containersIDs[ID / 10] >> (uint64_t)(ID % 10 * 6)) & (uint64_t)((1<<6) - 1);
}

static inline void addAimCellById(Map self, uint8_t element, uint8_t ID) {
    self->aimCellsIDs[ID / 10] &= ~((uint64_t)element << (uint64_t)(ID % 10 * 6));
    self->aimCellsIDs[ID / 10] |= ((uint64_t)element << (uint64_t)(ID % 10 * 6));
    self->aimCellsChecks[element >> 4] &= ~((uint64_t)15 << (uint64_t)((element & 15) << 2));
    self->aimCellsChecks[element >> 4] |= ((uint64_t)ID << (uint64_t)((element & 15) << 2));
}

static inline void addContainerAtID(Map self, uint8_t element, uint8_t ID) {
    self->containersChecks[element >> 4] ^= ((uint64_t)ID << (uint64_t)((element & 15) << 2));
}

static inline uint8_t getIdByAimCell(Map self, uint8_t element) {
    return (uint8_t)((self->aimCellsChecks[element >> 4] >> ((element & 15) << 2)) & 15ULL);
}

static inline uint8_t getAimCellById(Map self, uint8_t ID) {
    return (self->aimCellsIDs[ID / 10] >> (uint64_t)(ID % 10 * 6)) & (uint64_t)((1<<6) - 1);
}

void distances(Map self) {
    int8_t ddMatrix[1<<6][1<<6] = {{0}};
    for(int8_t i = 0; i < self->objectsCount; i++) {
        uint8_t container = getContainerById(self, i);
        for(int8_t j = 0; j < self->objectsCount; j++) {
            uint8_t aimCell = getAimCellById(self, j);
            uint8_t dd = bfsDistance(self, container, aimCell);
            ddMatrix[j][i] = dd;
            self->distMatrix[j][i] = j;
        }
    }

    for(int8_t i = 0; i < self->objectsCount; i++) {
        for(int8_t j = 0; j < self->objectsCount; j++) {
            for(int8_t k = j + 1; k < self->objectsCount; k++) {
                if(ddMatrix[j][i] > ddMatrix[k][i]) {
                    self->distMatrix[j][i] ^= self->distMatrix[k][i];
                    self->distMatrix[k][i] ^= self->distMatrix[j][i];
                    self->distMatrix[j][i] ^= self->distMatrix[k][i];

                    ddMatrix[j][i] ^= ddMatrix[k][i];
                    ddMatrix[k][i] ^= ddMatrix[j][i];
                    ddMatrix[j][i] ^= ddMatrix[k][i];
                }
            }
        }
    }
}

Map mm_Add(unsigned char **map, uint8_t sizeY, uint8_t sizeX) {
    Map self = (Map)malloc(sizeof(struct Map_t));
    memset(self, 0, sizeof(struct Map_t));
    int16_t maxSize = (1<<7);
    self->sizeY = sizeY;
    self->sizeX = sizeX;
    self->map = map;

    self->response = (uint8_t *)malloc(sizeof(uint8_t) * 10001);
    memset(self->response, 0, sizeof(uint8_t) * 10001);

    self->prealocatedMemoryStack = (struct Stack_t*)malloc(sizeof(struct Stack_t) * INITIAL_CAPACITY);
    memset(self->prealocatedMemoryStack, 0, sizeof(struct Stack_t) * INITIAL_CAPACITY);
    self->hashMap = new std::unordered_map<uint64_t, bool>();
    self->player = (Player)malloc(sizeof(struct Player_t));
    self->player->playGround = self;
    self->forbiddenPlaces = (uint8_t*)malloc(sizeof(uint8_t) * maxSize);
    memset(self->forbiddenPlaces, 0, sizeof(uint8_t) * maxSize);
    transformMap(self, self->map);
    getContainers(self);
    getAimCells(self);
    findAgent(self);
    clearMap(self);
    createLockers(self);
    createPrecald(self);
    findInteriorPoints(self);
    precalculateClosestBox(self);
    getWall(self);
    createHeuristicDistance(self);
    distances(self);
    return self;
}

void createBackTracking_t(Map self, int8_t k, bool *hashes, uint8_t *path, int32_t *parents,
                          uint8_t *actions, PQueue queue, struct Stack_t *first, uint8_t *isDone) {
    if(*isDone) {
        return ;
    }
    if(k == self->objectsCount) {
        mm_SetState(self, first);
        self->memIndex = 1;
        mm_SearchDataAstar(self, mm_GetHeuristicsPair, 8e5, path, parents, actions, queue, isDone);
    }
    for(int8_t j = 0; j < self->objectsCount; j++) {
        if(!hashes[self->distMatrix[j][k]]) {
            hashes[self->distMatrix[j][k]] = 1;
            self->pairs[1][k] = self->distMatrix[j][k];
            createBackTracking_t(self, k + 1, hashes, path, parents, actions, queue, first, isDone);
            hashes[self->distMatrix[j][k]] = 0;
        }
    }
}

void createBackTracking(Map self) {
    uint8_t *path = (uint8_t*)malloc(sizeof(uint8_t) * INITIAL_CAPACITY);
    int32_t *parents = (int32_t*)malloc(sizeof(int32_t) * INITIAL_CAPACITY);
    uint8_t *actions = (uint8_t*)malloc(sizeof(uint8_t) * (1<<16));
    PQueue queue = pq_New();
    struct Stack_t *stackElement = mm_GetStack(self, -1);
    mm_SetState(self, stackElement);
    self->memIndex = 1;
    bool hashes[1<<7] = {0};
    struct Stack_t *firstStackElement = mm_GetStack(self, -1);
    uint8_t isDone = 0;
    createBackTracking_t(self, 0, hashes, path, parents, actions, queue, firstStackElement, &isDone);
}

void playGame(uint8_t **bufferMap, uint8_t *response, uint8_t it_y, uint8_t it_x) {
    Map map = mm_Add(bufferMap, it_y, it_x);
    Start = clock();
    createBackTracking(map);
    memcpy(response, map->response, sizeof(uint8_t) * map->responseIndex);
}

int main() {
    // int bufferLength = 25;
    // char buffer[bufferLength];
    // unsigned char **bufferMap = (unsigned char **)malloc(sizeof(char*) * 25);
    // uint8_t it_y = 0;
    // uint8_t it_x = 0;
    // FILE *fd = fopen("input.txt", "r+");
    // for(int32_t i = 0; i < 10; i++) {
    //     bufferMap[i] = (unsigned char *)malloc(sizeof(char) * 25);
    //     memset(bufferMap[i], 0, sizeof(char) * 25);
    // }
    // while(fgets(buffer, 60, fd)) {
    //     uint8_t bufferSize = strlen(buffer);
    //     it_x = it_x < bufferSize ? bufferSize : it_x;
    //     for(uint8_t i = 0; i < it_x; i++) {
    //         bufferMap[it_y][i] = buffer[i];
    //     }
    //     it_y++;
    // }
    // fclose(fd);
    // uint8_t response[10004] = {0};
    // playGame(bufferMap, response, it_y, it_x);
    // printf("%s", response);

    // Map map = mm_Add(bufferMap, it_y, it_x);
    // Start = clock();
    // createBackTracking(map);
}

void map_Delete(Map self) {
    free(self->forbiddenPlaces);
    free(self->prealocatedMemoryStack);

}

#ifdef __cplusplus
extern "C" {
#endif

EMSCRIPTEN_KEEPALIVE void *allocBuffer(size_t size) {
  return malloc(size);
}

EMSCRIPTEN_KEEPALIVE uint8_t **allocMatr(uint8_t y, uint8_t x) {
   uint8_t **matr = (uint8_t **)malloc(sizeof(uint8_t **) * y);
   for(uint8_t i = 0; i < y; i++) {
       matr[i] = (uint8_t *)malloc(sizeof(uint8_t) * x);
       memset(matr[i], 0, sizeof(uint8_t) * x);
   }
   return matr;
}

EMSCRIPTEN_KEEPALIVE uint8_t setMatrAt(uint8_t **buffer, uint8_t i, uint8_t j, uint8_t value) {
   buffer[i][j] = value;
   return 1;
}

EMSCRIPTEN_KEEPALIVE uint8_t showMapAt(uint8_t **buffer) {
   for(uint8_t i = 0; i < 8; i++) {
       for(int8_t j = 0; j < 8; j++) {
           printf("%c", buffer[i][j]);
       }
       printf("\n");
   }
   return 1;
}

EMSCRIPTEN_KEEPALIVE uint8_t *getSokoResponse(uint8_t **buffer, uint8_t it_y, uint8_t it_x) {
  Map map = mm_Add(buffer, it_y, it_x);
  Start = clock();
  createBackTracking(map);
  uint8_t *response = (uint8_t *)malloc(sizeof(uint8_t) * map->responseIndex);
  memcpy(response, map->response, sizeof(uint8_t) * map->responseIndex);
//   mm_Show(map);
  for(int32_t i = 0; i < map->responseIndex; i++) {
      printf("%c", map->response[i]);
  }
  printf("\n");
  map_Delete(map);
  return response;
}

EMSCRIPTEN_KEEPALIVE uint8_t getCharAtPos(uint8_t *buffer, uint32_t index) {
  return buffer[index];
}

#ifdef __cplusplus
}
#endif