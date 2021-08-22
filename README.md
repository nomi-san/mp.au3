# mp.au3
Yet another multi-process library for AutoIt3

## features
- Easy to use
- High performance
- Support IPC, via COM object and message events

## usage

Just copy **mp.au3** and DLLs from [release](https://github.com/nomi-san/mp.au3/releases) into your project.

You can build DLL from source, e.g with **MSVC**:
```
$ cl /MT /LD /O2 mp.cc user32.lib kernel32.lib oleaut32.lib
```

## how it works?

We emulate the "fork process" technique with `CreateProcess()`.

```cpp
// Clone current process
CreateProcess(NULL, GetCommandLine()... CREATE_SUSPENDED
```

To preload data before initialization, we create process with flag `CREATE_SUSPENDED` to suspend it.
Then inject our DLL to it and write memory. Provide only a single DLL to help hacking memory easier.

```cpp
static int data;
...
WriteProcessMemory(process, &data, &data, sizeof(data) ...
```
- `data` is static and has same base address in different process
- So address `data` can be... `module.dll + &data`

On IPC, we provide a simple IDispatch object to manipulate data in main process.
In sub processes, we also use hacking memory.

For message events, we just use **headless window message** and `SendMessage()`.

## example

```au3
#NoTrayIcon
#include "mp.au3"

; Initialize.
_MP_Init()

; Get shared data object.
global $oData = _MP_SharedData()

; Check if the process is main.
if _MP_IsMain() then
	main()
else
	sub()
endIf

; Main process.
func main()
	; Set message.
	$oData.message = "Hello bro, I'm here."

	; Fork process.
	local $sub = _MP_Fork()

	; Show message box.
	MsgBox(0, 'Main process', 'Hi guy, sub process.')

	; Wait for sub process exit.
	_MP_Wait($sub)
endFunc

; Sub process.
func sub()
	Sleep(500)
	; Show message data.
	MsgBox(0, 'Sub process', $oData.message)
endFunc
```
