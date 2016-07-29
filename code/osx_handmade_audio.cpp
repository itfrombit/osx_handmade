


///////////////////////////////////////////////////////////////////////
// Audio
//
#if 1 // Handmade Hero Sound Buffer
OSStatus OSXAudioUnitCallback(void * inRefCon,
                              AudioUnitRenderActionFlags * ioActionFlags,
                              const AudioTimeStamp * inTimeStamp,
                              UInt32 inBusNumber,
                              UInt32 inNumberFrames,
                              AudioBufferList * ioData)
{
	// NOTE(jeff): Don't do anything too time consuming in this function.
	//             It is a high-priority "real-time" thread.
	//             Even too many printf calls can throw off the timing.
	#pragma unused(ioActionFlags)
	#pragma unused(inTimeStamp)
	#pragma unused(inBusNumber)

	//double currentPhase = *((double*)inRefCon);

	osx_sound_output* SoundOutput = ((osx_sound_output*)inRefCon);


	if (SoundOutput->ReadCursor == SoundOutput->WriteCursor)
	{
		SoundOutput->SoundBuffer.SampleCount = 0;
		//printf("AudioCallback: No Samples Yet!\n");
	}

	//printf("AudioCallback: SampleCount = %d\n", SoundOutput->SoundBuffer.SampleCount);

	int SampleCount = inNumberFrames;
	if (SoundOutput->SoundBuffer.SampleCount < inNumberFrames)
	{
		SampleCount = SoundOutput->SoundBuffer.SampleCount;
	}

	int16* outputBufferL = (int16 *)ioData->mBuffers[0].mData;
	int16* outputBufferR = (int16 *)ioData->mBuffers[1].mData;

	for (UInt32 i = 0; i < SampleCount; ++i)
	{
		outputBufferL[i] = *SoundOutput->ReadCursor++;
		outputBufferR[i] = *SoundOutput->ReadCursor++;

		if ((char*)SoundOutput->ReadCursor >= (char*)((char*)SoundOutput->CoreAudioBuffer + SoundOutput->SoundBufferSize))
		{
			//printf("Callback: Read cursor wrapped!\n");
			SoundOutput->ReadCursor = SoundOutput->CoreAudioBuffer;
		}
	}

	for (UInt32 i = SampleCount; i < inNumberFrames; ++i)
	{
		outputBufferL[i] = 0.0;
		outputBufferR[i] = 0.0;
	}

	return noErr;
}

#else // Test Sine Wave

OSStatus SineWaveRenderCallback(void * inRefCon,
                                AudioUnitRenderActionFlags * ioActionFlags,
                                const AudioTimeStamp * inTimeStamp,
                                UInt32 inBusNumber,
                                UInt32 inNumberFrames,
                                AudioBufferList * ioData)
{
	#pragma unused(ioActionFlags)
	#pragma unused(inTimeStamp)
	#pragma unused(inBusNumber)

	//double currentPhase = *((double*)inRefCon);

	osx_sound_output* SoundOutput = ((osx_sound_output*)inRefCon);

	int16* outputBuffer = (int16 *)ioData->mBuffers[0].mData;
	const double phaseStep = (SoundOutput->Frequency
							   / SoundOutput->SoundBuffer.SamplesPerSecond)
		                     * (2.0 * M_PI);

	for (UInt32 i = 0; i < inNumberFrames; i++)
	{
		outputBuffer[i] = 5000 * sin(SoundOutput->RenderPhase);
		SoundOutput->RenderPhase += phaseStep;
	}

	// Copy to the stereo (or the additional X.1 channels)
	for(UInt32 i = 1; i < ioData->mNumberBuffers; i++)
	{
		memcpy(ioData->mBuffers[i].mData, outputBuffer, ioData->mBuffers[i].mDataByteSize);
	}

	return noErr;
}

OSStatus SilentCallback(void* inRefCon,
                        AudioUnitRenderActionFlags* ioActionFlags,
                        const AudioTimeStamp* inTimeStamp,
                        UInt32 inBusNumber,
                        UInt32 inNumberFrames,
                        AudioBufferList* ioData)
{
	#pragma unused(inRefCon)
	#pragma unused(ioActionFlags)
	#pragma unused(inTimeStamp)
	#pragma unused(inBusNumber)

	//double currentPhase = *((double*)inRefCon);
	//osx_sound_output* SoundOutput = ((osx_sound_output*)inRefCon);

	Float32* outputBuffer = (Float32 *)ioData->mBuffers[0].mData;

	for (UInt32 i = 0; i < inNumberFrames; i++)
	{
		outputBuffer[i] = 0.0;
	}

	// Copy to the stereo (or the additional X.1 channels)
	for(UInt32 i = 1; i < ioData->mNumberBuffers; i++)
	{
		memcpy(ioData->mBuffers[i].mData, outputBuffer,
		       ioData->mBuffers[i].mDataByteSize);
	}

	return noErr;
}
#endif


#if 0 // Use AudioQueues
void OSXAudioQueueCallback(void* data, AudioQueueRef queue, AudioQueueBufferRef buffer)
{
	osx_sound_output* SoundOutput = ((osx_sound_output*)data);

	int16* outputBuffer = (int16 *)ioData->mBuffers[0].mData;
	const double phaseStep = (SoundOutput->Frequency / SoundOutput->SamplesPerSecond) * (2.0 * M_PI);

	for (UInt32 i = 0; i < inNumberFrames; i++)
	{
		outputBuffer[i] = 5000 * sin(SoundOutput->RenderPhase);
		SoundOutput->RenderPhase += phaseStep;
	}

	// Copy to the stereo (or the additional X.1 channels)
	for(UInt32 i = 1; i < ioData->mNumberBuffers; i++)
	{
		memcpy(ioData->mBuffers[i].mData, outputBuffer, ioData->mBuffers[i].mDataByteSize);
	}
}


void OSXInitCoreAudio(osx_sound_output* SoundOutput)
{
	SoundOutput->AudioDescriptor.mSampleRate       = SoundOutput->SamplesPerSecond;
	SoundOutput->AudioDescriptor.mFormatID         = kAudioFormatLinearPCM;
	SoundOutput->AudioDescriptor.mFormatFlags      = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
	SoundOutput->AudioDescriptor.mFramesPerPacket  = 1;
	SoundOutput->AudioDescriptor.mChannelsPerFrame = 2;
	SoundOutput->AudioDescriptor.mBitsPerChannel   = sizeof(int16) * 8;
	SoundOutput->AudioDescriptor.mBytesPerFrame    = sizeof(int16); // don't multiply by channel count with non-interleaved!
	SoundOutput->AudioDescriptor.mBytesPerPacket   = SoundOutput->AudioDescriptor.mFramesPerPacket * SoundOutput->AudioDescriptor.mBytesPerFrame;

	uint32 err = AudioQueueNewOutput(&SoundOutput->AudioDescriptor, OSXAudioQueueCallback, SoundOutput, NULL, 0, 0, &SoundOutput->AudioQueue);
	if (err)
	{
		printf("Error in AudioQueueNewOutput\n");
	}

}

#else // Use raw AudioUnits

void OSXInitCoreAudio(osx_sound_output* SoundOutput)
{
	AudioComponentDescription acd;
	acd.componentType         = kAudioUnitType_Output;
	acd.componentSubType      = kAudioUnitSubType_DefaultOutput;
	acd.componentManufacturer = kAudioUnitManufacturer_Apple;

	AudioComponent outputComponent = AudioComponentFindNext(NULL, &acd);

	AudioComponentInstanceNew(outputComponent, &SoundOutput->AudioUnit);
	AudioUnitInitialize(SoundOutput->AudioUnit);

#if 1 // uint16
	//AudioStreamBasicDescription asbd;
	SoundOutput->AudioDescriptor.mSampleRate       = SoundOutput->SoundBuffer.SamplesPerSecond;
	SoundOutput->AudioDescriptor.mFormatID         = kAudioFormatLinearPCM;
	SoundOutput->AudioDescriptor.mFormatFlags      = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved | kAudioFormatFlagIsPacked;
	SoundOutput->AudioDescriptor.mFramesPerPacket  = 1;
	SoundOutput->AudioDescriptor.mChannelsPerFrame = 2; // Stereo
	SoundOutput->AudioDescriptor.mBitsPerChannel   = sizeof(int16) * 8;
	SoundOutput->AudioDescriptor.mBytesPerFrame    = sizeof(int16); // don't multiply by channel count with non-interleaved!
	SoundOutput->AudioDescriptor.mBytesPerPacket   = SoundOutput->AudioDescriptor.mFramesPerPacket * SoundOutput->AudioDescriptor.mBytesPerFrame;
#else // floating point - this is the "native" format on the Mac
	AudioStreamBasicDescription asbd;
	SoundOutput->AudioDescriptor.mSampleRate       = SoundOutput->SamplesPerSecond;
	SoundOutput->AudioDescriptor.mFormatID         = kAudioFormatLinearPCM;
	SoundOutput->AudioDescriptor.mFormatFlags      = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
	SoundOutput->AudioDescriptor.mFramesPerPacket  = 1;
	SoundOutput->AudioDescriptor.mChannelsPerFrame = 2;
	SoundOutput->AudioDescriptor.mBitsPerChannel   = sizeof(Float32) * 8; // 1 * sizeof(Float32) * 8;
	SoundOutput->AudioDescriptor.mBytesPerFrame    = sizeof(Float32);
	SoundOutput->AudioDescriptor.mBytesPerPacket   = SoundOutput->AudioDescriptor.mFramesPerPacket * SoundOutput->AudioDescriptor.mBytesPerFrame;
#endif


	// TODO(jeff): Add some error checking...
	AudioUnitSetProperty(SoundOutput->AudioUnit,
                         kAudioUnitProperty_StreamFormat,
                         kAudioUnitScope_Input,
                         0,
                         &SoundOutput->AudioDescriptor,
                         sizeof(SoundOutput->AudioDescriptor));

	AURenderCallbackStruct cb;
	cb.inputProc = OSXAudioUnitCallback;
	cb.inputProcRefCon = SoundOutput;

	AudioUnitSetProperty(SoundOutput->AudioUnit,
                         kAudioUnitProperty_SetRenderCallback,
                         kAudioUnitScope_Global,
                         0,
                         &cb,
                         sizeof(cb));

	AudioOutputUnitStart(SoundOutput->AudioUnit);
}


void OSXStopCoreAudio(osx_sound_output* SoundOutput)
{
	printf("Stopping Core Audio\n");
	AudioOutputUnitStop(SoundOutput->AudioUnit);
	AudioUnitUninitialize(SoundOutput->AudioUnit);
	AudioComponentInstanceDispose(SoundOutput->AudioUnit);
}

#endif



