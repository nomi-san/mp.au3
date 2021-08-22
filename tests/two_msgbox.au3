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