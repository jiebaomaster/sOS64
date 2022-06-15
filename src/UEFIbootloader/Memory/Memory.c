#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Uefi.h>

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle,
                           IN EFI_SYSTEM_TABLE *SystemTable) {
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gGraphicsOutput = 0;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info = 0;
  UINTN InfoSize = 0;
  int i = 0;
  // 取得 EFI_GRAPHICS_OUTPUT_PROTOCOL 协议
  gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL,
                      (VOID **)&gGraphicsOutput);
  // 显示当前的显示模式等信息
  Print(L"Current "
        L"Mode:%02d,Version:%x,Format:%d,Horizontal:%d,Vertical:%d,ScanLine:%d,"
        L"FrameBufferBase:%010lx,FrameBufferSize:%010lx\n",
        gGraphicsOutput->Mode->Mode, gGraphicsOutput->Mode->Info->Version,
        gGraphicsOutput->Mode->Info->PixelFormat,
        gGraphicsOutput->Mode->Info->HorizontalResolution,
        gGraphicsOutput->Mode->Info->VerticalResolution,
        gGraphicsOutput->Mode->Info->PixelsPerScanLine,
        gGraphicsOutput->Mode->FrameBufferBase,
        gGraphicsOutput->Mode->FrameBufferSize);

  long H_V_Resolution = gGraphicsOutput->Mode->Info->HorizontalResolution *
                        gGraphicsOutput->Mode->Info->VerticalResolution;
  int MaxResolutionMode = gGraphicsOutput->Mode->Mode;
  // 遍历查询图形设备支持的所有显示模式
  for (i = 0; i < gGraphicsOutput->Mode->MaxMode; i++) {
    gGraphicsOutput->QueryMode(gGraphicsOutput, i, &InfoSize, &Info);
    Print(L"Mode:%02d,Version:%x,Format:%d,Horizontal:%d,Vertical:%d,ScanLine:%"
          L"d\n",
          i, Info->Version, Info->PixelFormat, Info->HorizontalResolution,
          Info->VerticalResolution, Info->PixelsPerScanLine);
    // 选择分辨率最大的显示模式
    if ((Info->PixelFormat == 1) &&
        (Info->HorizontalResolution * Info->VerticalResolution >
         H_V_Resolution)) {
      H_V_Resolution = Info->HorizontalResolution * Info->VerticalResolution;
      MaxResolutionMode = i;
    }
    // FreePool 方法释放显示模式信息结构所占用的内存空间以免内存溢出
    gBS->FreePool(Info);
  }
  // 设置图形设备的显示模式
  gGraphicsOutput->SetMode(gGraphicsOutput, MaxResolutionMode);
  gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL,
                      (VOID **)&gGraphicsOutput);
  // 显示设置后的显示模式等信息
  Print(L"Current "
        L"Mode:%02d,Version:%x,Format:%d,Horizontal:%d,Vertical:%d,ScanLine:%d,"
        L"FrameBufferBase:%010lx,FrameBufferSize:%010lx\n",
        gGraphicsOutput->Mode->Mode, gGraphicsOutput->Mode->Info->Version,
        gGraphicsOutput->Mode->Info->PixelFormat,
        gGraphicsOutput->Mode->Info->HorizontalResolution,
        gGraphicsOutput->Mode->Info->VerticalResolution,
        gGraphicsOutput->Mode->Info->PixelsPerScanLine,
        gGraphicsOutput->Mode->FrameBufferBase,
        gGraphicsOutput->Mode->FrameBufferSize);

  gBS->CloseProtocol(gGraphicsOutput, &gEfiGraphicsOutputProtocolGuid,
                     ImageHandle, NULL);

  UINTN MemMapSize = 0;
  EFI_MEMORY_DESCRIPTOR *MemMap = 0;
  UINTN MapKey = 0;
  UINTN DescriptorSize = 0;
  UINT32 DesVersion = 0;

  Print(L"Get EFI_MEMORY_DESCRIPTOR Structure\n");
  // 查询物理地址空间信息占用的内存空间大小
  gBS->GetMemoryMap(&MemMapSize, MemMap, &MapKey, &DescriptorSize, &DesVersion);
  // 为内存描述符结构数组分配存储空间
  gBS->AllocatePool(EfiRuntimeServicesData, MemMapSize, (VOID **)&MemMap);
  // 获取物理地址空间信息
  gBS->GetMemoryMap(&MemMapSize, MemMap, &MapKey, &DescriptorSize, &DesVersion);
  // 打印所有物理地址空间信息
  for (i = 0; i < MemMapSize / DescriptorSize; i++) {
    EFI_MEMORY_DESCRIPTOR *MMap =
        (EFI_MEMORY_DESCRIPTOR *)(((CHAR8 *)MemMap) + i * DescriptorSize);
    Print(L"MemoryMap %4d %10d (%10lx~%10lx) %016lx\n", MMap->Type,
          MMap->NumberOfPages, MMap->PhysicalStart,
          MMap->PhysicalStart + (MMap->NumberOfPages << 12), MMap->Attribute);
  }
  gBS->FreePool(MemMap);

  return EFI_SUCCESS;
}