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
	; Set initial value.
	$oData.count = 0

	; Fork process.
	_MP_Fork()

	; Create GUI.
	local $gui = GUICreate('Main GUI', 300, 200)
	local $txt = GUICtrlCreateLabel('', 30, 30, 100, 30)
	GUISetState(@SW_SHOW)

	; Message loop.
	while 1
		switch GUIGetMsg()
			case -3
				GUISetState(@SW_HIDE)
				_MP_WaitAll()
				Exit
		endSwitch

		; Update count value.
		GUICtrlSetData($txt, 'Count: ' & $oData.count)
		Sleep(50)
	wEnd
endFunc

; Sub process.
func sub()
	; Create GUI.
	local $gui = GUICreate('Sub GUI', 200, 200)
	local $btn = GUICtrlCreateButton('Click me', 30, 30, 70, 30)
	GUISetState(@SW_SHOW)

	while 1
		switch GUIGetMsg()
			case -3
				GUISetState(@SW_HIDE)
				Exit
			case $btn
				; Increase count.
				$oData.count += 1
		endSwitch

		Sleep(20)
	wEnd
endFunc