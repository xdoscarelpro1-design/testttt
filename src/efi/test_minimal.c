#include <efi.h>
#include <efilib.h>

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    SystemTable->ConOut->OutputString(SystemTable->ConOut,
        L"PHANTOM TEST - IF YOU SEE THIS, EFI WORKS!\r\n\r\n");
    SystemTable->ConOut->OutputString(SystemTable->ConOut,
        L"Press any key to reboot...\r\n");

    EFI_INPUT_KEY Key;
    UINTN Idx;
    SystemTable->BootServices->WaitForEvent(
        1, &SystemTable->ConIn->WaitForKey, &Idx);
    SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key);

    SystemTable->RuntimeServices->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
    return EFI_SUCCESS;
}
