#NoTrayIcon
#include "mp.au3"

; Initialize.
_MP_Init()

; Enum message.
local enum $_INCREASE, $_DECREASE

local $_label
local $_count = 0

; Check if the process is main.
if _MP_IsMain() then
	main()
else
	sub()
endIf

; Main process.
func main()
	; Set listener.
	local $hClbk = _MP_OnSub(SubListener)

	; Fork process.
	_MP_Fork()

	; Create GUI.
	local $gui = GUICreate('Main GUI', 300, 200)
	$_label = GUICtrlCreateLabel('', 30, 30, 100, 30)

	UpdateCount()
	GUISetState(@SW_SHOW)

	; Message loop.
	while 1
		switch GUIGetMsg()
			case -3
				GUISetState(@SW_HIDE)
				_MP_WaitAll()
				; Remove callback.
				_MP_OffSub($hClbk)
				Exit
		endSwitch
		Sleep(20)
	wEnd
endFunc

func UpdateCount()
	GUICtrlSetData($_label, 'Count: ' & $_count)
endFunc

func SubListener($iPoc, $iMsg)
	if $iMsg == $_INCREASE then
		; Increase count.
		$_count += 1
		UpdateCount()
	elseIf $iMsg == $_DECREASE then
		; Decrease count
		$_count -= 1
		UpdateCount()
	endIf
endFunc

; Sub process.
func sub()
	; Create GUI.
	local $gui = GUICreate('Sub GUI', 200, 200)
	local $inc = GUICtrlCreateButton('+', 30, 30, 70, 30)
	local $dec = GUICtrlCreateButton('-', 30, 80, 70, 30)
	GUISetState(@SW_SHOW)

	while 1
		switch GUIGetMsg()
			case -3
				GUISetState(@SW_HIDE)
				Exit
			case $inc
				_MP_SendToMain($_INCREASE)
			case $dec
				_MP_SendToMain($_DECREASE)
		endSwitch
		Sleep(20)
	wEnd
endFunc