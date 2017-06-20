//
// Created by Linus on 2017-03-03.
//

#include <stdlib.h>
#include "FramePool.h"

FramePool::FramePool(int size, int frameSize) 
{
    numberOfFrames = size;

    //Pre-allocate frames for pool
    for (int i = 0; i < numberOfFrames; i++){

        FrameObject_t *frame = (FrameObject_t*)malloc((sizeof(FrameObject_t)));
        frame->frame = (uint8_t*)(malloc(frameSize));
        frame->pts = 0;

        freeFrames.push(frame);
    }
}

FramePool::~FramePool() 
{
    while (freeFrames.size() > 0) 
	{
        FrameObject_t *frame = freeFrames.front();
        freeFrames.pop();

        free(frame->frame);
        free(frame);
    }
}

FrameObject_t* FramePool::popFrame() 
{
    FrameObject_t *frame = freeFrames.front();
    freeFrames.pop();

    return frame;
}

void FramePool::pushFrame(FrameObject_t *frame) 
{
    freeFrames.push(frame);
}

int FramePool::framesAvailable() 
{
    EnterRead();
    int size = freeFrames.size();
    ExitRead();

    return size;
}

void FramePool::EnterRead() {
	#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
    EncodeThreadSyncObject *syncObject = GetSyncObject();
    pthread_mutex_lock(&syncObject->Mutex);
	#endif
}

void FramePool::ExitRead() {
	#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
    EncodeThreadSyncObject *syncObject = GetSyncObject();
    pthread_mutex_unlock(&syncObject->Mutex);
	#endif
}

#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
EncodeThreadSyncObject* FramePool::GetSyncObject() 
{
    return &syncObject;
}
#endif
