//
// Created by Linus on 2017-03-03.
//

#pragma once

#include <queue>
#include <stdint.h>

#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#include <pthread.h>

struct EncodeThreadSyncObject
{
    pthread_cond_t ConditionVariable;
    pthread_mutex_t Mutex;

    EncodeThreadSyncObject()
    {
        ConditionVariable = PTHREAD_COND_INITIALIZER;
        Mutex = PTHREAD_MUTEX_INITIALIZER;
    }
};
#endif

typedef struct FrameObject
{
    uint8_t *frame;
    int64_t pts;
} FrameObject_t;

class FramePool {
    int numberOfFrames;
    std::queue <FrameObject_t*> freeFrames;

	#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
    struct EncodeThreadSyncObject syncObject;
	#endif

    void EnterRead();
    void ExitRead();

public:

    FramePool(int size, int frameSize);
    ~FramePool();

    FrameObject_t *popFrame();
    void pushFrame(FrameObject_t *frame);
    int framesAvailable();

	#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
    EncodeThreadSyncObject* GetSyncObject();
	#endif
};
