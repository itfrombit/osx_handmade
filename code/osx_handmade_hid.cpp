
///////////////////////////////////////////////////////////////////////
// HID input
//


#define OSX_HANDMADE_MAX_HID_DEVICES 32
#define OSX_HANDMADE_MAX_HID_DEVICE_ELEMENTS 256

#define OSX_HANDMADE_MAX_HID_MANUFACTURER_LEN 256
#define OSX_HANDMADE_MAX_HID_PRODUCT_LEN 256


typedef struct osx_hid_element
{
	IOHIDElementRef ElementRef;
	IOHIDElementCookie Cookie;
	long Type;
	long Page;
	long Usage;
	long Min;
	long Max;
} osx_hid_element;


typedef struct osx_hid_device
{
	IOHIDDeviceRef DeviceRef;

	osx_game_data* GameData;

	char Manufacturer[OSX_HANDMADE_MAX_HID_MANUFACTURER_LEN];
	char Product[OSX_HANDMADE_MAX_HID_PRODUCT_LEN];

	int ElementCount;
	osx_hid_element Elements[OSX_HANDMADE_MAX_HID_DEVICE_ELEMENTS];
} osx_hid_device;


static osx_hid_device* HIDDevices[OSX_HANDMADE_MAX_HID_DEVICES];
static int HIDDeviceCount = 0;


void OSXHIDElementInitialize(osx_hid_element* Element,
							 IOHIDElementCookie Cookie,
							 long Type, long Page, long Usage, long Min, long Max)
{
	Element->Cookie = Cookie;
	Element->Type = Type;
	Element->Page = Page;
	Element->Usage = Usage;
	Element->Min = Min;
	Element->Max = Max;
}


osx_hid_device* OSXHIDDeviceCreate(IOHIDDeviceRef DeviceRef,
								   const char* Manufacturer,
								   const char* Product,
								   osx_game_data* GameData)
{
	Assert(HIDDeviceCount < (OSX_HANDMADE_MAX_HID_DEVICES - 1));

	osx_hid_device* Device = (osx_hid_device*)malloc(sizeof(osx_hid_device));

	Device->DeviceRef = DeviceRef;

	strncpy(Device->Manufacturer, Manufacturer, OSX_HANDMADE_MAX_HID_MANUFACTURER_LEN);
	strncpy(Device->Product, Product, OSX_HANDMADE_MAX_HID_PRODUCT_LEN);

	Device->ElementCount = 0;

	Device->GameData = GameData;

	HIDDevices[HIDDeviceCount++] = Device;

	return Device;
}





void OSXHIDAction(void* Context, IOReturn Result, void* Sender, IOHIDValueRef Value)
{
	#pragma unused(Result)
	#pragma unused(Sender)

	osx_hid_device* Device = (osx_hid_device*)Context;

	// NOTE(jeff): Check suggested by Filip to prevent an access violation when
	// using a PS3 controller.
	// TODO(jeff): Investigate this further...
	if (IOHIDValueGetLength(Value) > 2)
	{
		//printf("OSXHIDAction: value length > 2: %ld\n", IOHIDValueGetLength(value));
		return;
	}

	IOHIDElementRef ElementRef = IOHIDValueGetElement(Value);
	/*
	if (CFGetTypeID(ElementRef) != IOHIDElementGetTypeID())
	{
		return;
	}
	*/

	IOHIDElementCookie Cookie = IOHIDElementGetCookie(ElementRef);
	int UsagePage = IOHIDElementGetUsagePage(ElementRef);
	int Usage = IOHIDElementGetUsage(ElementRef);

	int ElementValue = IOHIDValueGetIntegerValue(Value);


	osx_game_data* GameData = Device->GameData;

	osx_hid_element* Element = NULL;

	for (int i = 0; i < Device->ElementCount; ++i)
	{
		if (Device->Elements[i].Cookie == Cookie)
		{
			Element = &(Device->Elements[i]);
			break;
		}
	}

	if (Element == NULL)
	{
		printf("Could not find matching element for device\n");
		return;
	}
	else
	{
#if 0
		printf("HID Event: Cookie: %ld  Page/Usage = %ld/%ld  Min/Max = %ld/%ld  Value = %d\n",
				(long)Element->Cookie,
				Element->Page,
				Element->Usage,
				Element->Min,
				Element->Max,
				ElementValue);
#endif
	}

	// NOTE(jeff): This is just for reference. From the USB HID Usage Tables spec:
	// Usage Pages:
	//   1 - Generic Desktop (mouse, joystick)
	//   2 - Simulation Controls
	//   3 - VR Controls
	//   4 - Sports Controls
	//   5 - Game Controls
	//   6 - Generic Device Controls (battery, wireless, security code)
	//   7 - Keyboard/Keypad
	//   8 - LED
	//   9 - Button
	//   A - Ordinal
	//   B - Telephony
	//   C - Consumer
	//   D - Digitizers
	//  10 - Unicode
	//  14 - Alphanumeric Display
	//  40 - Medical Instrument

	if (UsagePage == 1) // Generic Desktop Page
	{
		int HatDelta = 16;

		float NormalizedValue = 0.0;
		if (Element->Max != Element->Min)
		{
			NormalizedValue = (float)(ElementValue - Element->Min) / (float)(Element->Max - Element->Min);
		}
		float ScaledMin = -25.0;
		float ScaledMax = 25.0;

		int ScaledValue = ScaledMin + NormalizedValue * (ScaledMax - ScaledMin);

		//printf("page:usage = %d:%d  value = %ld  ", usagePage, usage, elementValue);
		switch(Usage)
		{
			case 0x30: // x
				GameData->HIDX = ScaledValue;
				//printf("[x] scaled = %d\n", view->_HIDX);
				break;

			case 0x31: // y
				GameData->HIDY = ScaledValue;
				//printf("[y] scaled = %d\n", view->_HIDY);
				break;

			case 0x32: // z
				//GameData->HIDX = ScaledValue;
				//printf("[z] scaled = %d\n", view->_HIDX);
				break;

			case 0x35: // rz
				//GameData->HIDY = ScaledValue;
				//printf("[rz] scaled = %d\n", view->_HIDY);
				break;

			case 0x39: // Hat 0 = up, 2 = right, 4 = down, 6 = left, 8 = centered
			{
				game_controller_input* Controller = &GameData->NewInput->Controllers[0];

				OSXProcessKeyboardMessage(&Controller->MoveUp, 0);
				OSXProcessKeyboardMessage(&Controller->MoveDown, 0);
				OSXProcessKeyboardMessage(&Controller->MoveLeft, 0);
				OSXProcessKeyboardMessage(&Controller->MoveRight, 0);

				//printf("[hat] ");
				switch(ElementValue)
				{
					case 0:
						GameData->HIDX = 0;
						GameData->HIDY = -HatDelta;
						OSXProcessKeyboardMessage(&Controller->MoveUp, 1);
						//printf("n\n");
						break;

					case 1:
						GameData->HIDX = HatDelta;
						GameData->HIDY = -HatDelta;
						OSXProcessKeyboardMessage(&Controller->MoveUp, 1);
						OSXProcessKeyboardMessage(&Controller->MoveRight, 1);
						//printf("ne\n");
						break;

					case 2:
						GameData->HIDX = HatDelta;
						GameData->HIDY = 0;
						OSXProcessKeyboardMessage(&Controller->MoveRight, 1);
						//printf("e\n");
						break;

					case 3:
						GameData->HIDX = HatDelta;
						GameData->HIDY = HatDelta;
						OSXProcessKeyboardMessage(&Controller->MoveRight, 1);
						OSXProcessKeyboardMessage(&Controller->MoveDown, 1);
						//printf("se\n");
						break;

					case 4:
						GameData->HIDX = 0;
						GameData->HIDY = HatDelta;
						OSXProcessKeyboardMessage(&Controller->MoveDown, 1);
						//printf("s\n");
						break;

					case 5:
						GameData->HIDX = -HatDelta;
						GameData->HIDY = HatDelta;
						OSXProcessKeyboardMessage(&Controller->MoveDown, 1);
						OSXProcessKeyboardMessage(&Controller->MoveLeft, 1);
						//printf("sw\n");
						break;

					case 6:
						GameData->HIDX = -HatDelta;
						GameData->HIDY = 0;
						OSXProcessKeyboardMessage(&Controller->MoveLeft, 1);
						//printf("w\n");
						break;

					case 7:
						GameData->HIDX = -HatDelta;
						GameData->HIDY = -HatDelta;
						OSXProcessKeyboardMessage(&Controller->MoveLeft, 1);
						OSXProcessKeyboardMessage(&Controller->MoveUp, 1);
						//printf("nw\n");
						break;

					case 8:
						GameData->HIDX = 0;
						GameData->HIDY = 0;
						//printf("up\n");
						break;
				}

			} break;

			default:
				break;
		}
	}
	else if (UsagePage == 9) // Buttons
	{
		game_controller_input* Controller = &GameData->NewInput->Controllers[0];

		if ((ElementValue == 0) || (ElementValue == 1))
		{
			GameData->HIDButtons[Usage] = ElementValue;

			if (ElementValue == 1) printf("Button %d pressed\n", Usage);

			switch(Usage)
			{
				case 1:
					OSXProcessKeyboardMessage(&Controller->ActionLeft, ElementValue);
					break;

				case 2:
					OSXProcessKeyboardMessage(&Controller->ActionDown, ElementValue);
					break;

				case 3:
					OSXProcessKeyboardMessage(&Controller->ActionRight, ElementValue);
					break;

				case 4:
					OSXProcessKeyboardMessage(&Controller->ActionUp, ElementValue);
					break;

				case 9:
					OSXProcessKeyboardMessage(&Controller->Back, ElementValue);
					break;

				case 10:
					OSXProcessKeyboardMessage(&Controller->Start, ElementValue);
					break;

				default:
					break;
			}
		}
		else
		{
			printf("Gamepad Element: Cookie: %ld  Page: %d  Usage: %d  Value: %d  _HIDX: %d\n",
				   (long)Cookie, UsagePage, Usage, ElementValue, GameData->HIDX);
		}
	}
	else if (UsagePage == 7) // Keyboard
	{
		// NOTE(jeff): Moved to main loop event processing...
	}
	else
	{
		//printf("Gamepad Element: %@  Type: %d  Page: %d  Usage: %d  Name: %@  Cookie: %i  Value: %ld  _HIDX: %d\n",
		//	  element, type, usagePage, usage, name, cookie, elementValue, GameData->HIDX);
	}
}




void OSXHIDAdded(void* Context, IOReturn Result, void* Sender, IOHIDDeviceRef DeviceRef)
{
	#pragma unused(Result)
	#pragma unused(Sender)

	osx_game_data* GameData = (osx_game_data*)Context;

	CFStringRef ManufacturerCFSR = (CFStringRef)IOHIDDeviceGetProperty(DeviceRef, CFSTR(kIOHIDManufacturerKey));
	CFStringRef ProductCFSR = (CFStringRef)IOHIDDeviceGetProperty(DeviceRef, CFSTR(kIOHIDProductKey));

	const char* Manufacturer = CFStringGetCStringPtr(ManufacturerCFSR, kCFStringEncodingMacRoman);
	const char* Product = CFStringGetCStringPtr(ProductCFSR, kCFStringEncodingMacRoman);

	if (Manufacturer == NULL) Manufacturer = "[unknown]";
	if (Product == NULL) Product = "[unknown]";

	printf("Gamepad was detected: %s %s", Manufacturer, Product);


	osx_hid_device* Device = OSXHIDDeviceCreate(DeviceRef, Manufacturer, Product, GameData);


	CFArrayRef ElementArray = IOHIDDeviceCopyMatchingElements(DeviceRef, NULL, kIOHIDOptionsTypeNone);
	int ElementArrayCount = CFArrayGetCount(ElementArray);

	for (int i = 0; i < ElementArrayCount; ++i)
	{
		IOHIDElementRef ElementRef = (IOHIDElementRef)CFArrayGetValueAtIndex(ElementArray, i);

		IOHIDElementCookie Cookie = IOHIDElementGetCookie(ElementRef);
		IOHIDElementType ElementType = IOHIDElementGetType(ElementRef);

#if 0
		switch(ElementType)
		{
			case kIOHIDElementTypeInput_Misc:      printf("[misc] ");     break;
			case kIOHIDElementTypeInput_Button:    printf("[button] ");   break;
			case kIOHIDElementTypeInput_Axis:      printf("[axis] ");     break;
			case kIOHIDElementTypeInput_ScanCodes: printf("[scancode] "); break;
			default: continue;
		}
#endif

		uint32_t ReportSize = IOHIDElementGetReportSize(ElementRef);
		uint32_t ReportCount = IOHIDElementGetReportCount(ElementRef);
		if ((ReportSize * ReportCount) > 64)
		{
			continue;
		}

		uint32_t UsagePage = IOHIDElementGetUsagePage(ElementRef);
		uint32_t Usage = IOHIDElementGetUsage(ElementRef);

		if (!UsagePage || !Usage)
		{
			continue;
		}
		if (Usage == -1)
		{
			continue;
		}

		CFIndex LogicalMin = IOHIDElementGetLogicalMin(ElementRef);
		CFIndex LogicalMax = IOHIDElementGetLogicalMax(ElementRef);

		osx_hid_element* Element = &(Device->Elements[Device->ElementCount++]);
		OSXHIDElementInitialize(Element, Cookie, ElementType, UsagePage, Usage, LogicalMin, LogicalMax);
	}

	CFRelease(ElementArray);

	IOHIDDeviceRegisterInputValueCallback(DeviceRef, OSXHIDAction, Device);
}



void OSXHIDRemoved(void* context, IOReturn result, void* sender, IOHIDDeviceRef device)
{
	#pragma unused(context)
	#pragma unused(result)
	#pragma unused(sender)
	#pragma unused(device)

	printf("Gamepad was unplugged\n");
}



void OSXSetupGamepad(osx_game_data* game_data)
{
	game_data->HIDManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

	if (game_data->HIDManager)
	{
		CFStringRef keys[2];
		keys[0] = CFSTR(kIOHIDDeviceUsagePageKey);
		keys[1] = CFSTR(kIOHIDDeviceUsageKey);


		int usageValues[3] = {
			kHIDUsage_GD_Joystick,
			kHIDUsage_GD_GamePad,
			kHIDUsage_GD_MultiAxisController
		};
		int usageValuesCount = sizeof(usageValues) / sizeof(int);

		CFDictionaryRef dictionaries[usageValuesCount];

		for (int i = 0; i < usageValuesCount; ++i)
		{
			CFNumberRef values[2];

			int pageGDValue = kHIDPage_GenericDesktop;
			values[0] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pageGDValue);
			values[1] = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &(usageValues[i]));

			dictionaries[i] = CFDictionaryCreate(kCFAllocatorDefault,
												 (const void**)keys,
												 (const void**)values,
												 2,
												 &kCFTypeDictionaryKeyCallBacks,
												 &kCFTypeDictionaryValueCallBacks);
			CFRelease(values[0]);
			CFRelease(values[1]);
		}

#if 0
		NSArray* criteria = @[ @{ [NSString stringWithUTF8String:kIOHIDDeviceUsagePageKey]:
									[NSNumber numberWithInt:kHIDPage_GenericDesktop],
								[NSString stringWithUTF8String:kIOHIDDeviceUsageKey]:
									[NSNumber numberWithInt:kHIDUsage_GD_Joystick]
								},
							@{ (NSString*)CFSTR(kIOHIDDeviceUsagePageKey):
									[NSNumber numberWithInt:kHIDPage_GenericDesktop],
								(NSString*)CFSTR(kIOHIDDeviceUsageKey):
									[NSNumber numberWithInt:kHIDUsage_GD_GamePad]
								},
							@{ (NSString*)CFSTR(kIOHIDDeviceUsagePageKey):
									[NSNumber numberWithInt:kHIDPage_GenericDesktop],
								(NSString*)CFSTR(kIOHIDDeviceUsageKey):
									[NSNumber numberWithInt:kHIDUsage_GD_MultiAxisController]
							   }
#if 0
							   ,
							@{ (NSString*)CFSTR(kIOHIDDeviceUsagePageKey):
									[NSNumber numberWithInt:kHIDPage_GenericDesktop],
								(NSString*)CFSTR(kIOHIDDeviceUsageKey):
									[NSNumber numberWithInt:kHIDUsage_GD_Keyboard]
							   }
#endif
							];
#endif

		CFArrayRef criteria = CFArrayCreate(kCFAllocatorDefault,
											(const void**)dictionaries,
											usageValuesCount,
											&kCFTypeArrayCallBacks);


		// NOTE(jeff): These all return void, so no error checking...
		IOHIDManagerSetDeviceMatchingMultiple(game_data->HIDManager, criteria);
		IOHIDManagerRegisterDeviceMatchingCallback(game_data->HIDManager, OSXHIDAdded, (void*)game_data);
		IOHIDManagerRegisterDeviceRemovalCallback(game_data->HIDManager, OSXHIDRemoved, (void*)game_data);
		IOHIDManagerScheduleWithRunLoop(game_data->HIDManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

		if (IOHIDManagerOpen(game_data->HIDManager, kIOHIDOptionsTypeNone) == kIOReturnSuccess)
		{
			//IOHIDManagerRegisterInputValueCallback(game_data->HIDManager, OSXHIDAction, (void*)game_data);
		}
		else
		{
			// TODO(jeff): Diagnostic
		}


		for (int i = 0; i < usageValuesCount; ++i)
		{
			CFRelease(dictionaries[i]);
		}

		CFRelease(criteria);
	}
	else
	{
		// TODO(jeff): Diagnostic
	}
}


