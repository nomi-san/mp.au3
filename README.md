# mp.au3
Yet another multi-process library for AutoIt3

// working in progress

## example

```au3
#include "mp.au3"

; Initialize.
_MP_Init()

; Create shared data object.
global $oData = _MP_SharedData('data')

; Check if the process is main.
if _MP_IsMain() then
	main()
else
	sub()
endIf

; Main process.
func main()
	; Fork process.
	local $sub = _MP_Fork()

	; Set message.
	$oData.message = 'Hi main process.'

	; Show message box.
	MsgBox(0, 'Main process', 'Hello bro, sub process.')

	; Wait for sub process exit.
	_MP_Wait($sub)
endFunc

; Sub process.
func sub()
	; Show message data.
	MsgBox(0, 'Sub process', $oData.message)
endFunc
```
