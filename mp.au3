;~	Author: 	wuuyi123@gmail.com
;~	Repo:		https://github.com/nomi-san/mp.au3
;~	Release:	2021-08-22

#include-once

local $__MP_hDll = null

;~	Initialize the library.
;~  This function must be called on top-level of code.
;~  Returns
;~		0 	- Fail
;~		1 	- Success
func _MP_Init()
	local $sDll = @AutoItX64 ? 'mp.x64.dll' : 'mp.dll'
	if not FileExists($sDll) then return 0
	$__MP_hDll = DllOpen($sDll)
	if $__MP_hDll == null or $__MP_hDll == -1 then return 0
	local $aRet = DllCall($__MP_hDll, 'int:cdecl', 'MP_Init')
	if not IsArray($aRet) or @error then return 0
	return $aRet[0]
endFunc

;~	Check if current process is main.
;~  Returns
;~  	1 	- Main process
;~		0 	- Sub process
;		-99	- Error
func _MP_IsMain()
	local $aRet = DllCall($__MP_hDll, 'int:cdecl', 'MP_IsMain')
	if not IsArray($aRet) or @error then return -99
	return $aRet[0]
endFunc

;~	Get index of current process.
;~	This function must be called on top-level of code to determine which the current process is.
;~  Returns
;~  	-1 	- Main process
;~		0+ 	- Sub process
;~		-99 - Error
func _MP_Index()
	local $aRet = DllCall($__MP_hDll, 'int:cdecl', 'MP_IsMain')
	if not IsArray($aRet) or @error then return -99
	return $aRet[0]
endFunc

;~	Fork the main process.
;~	This function must be called in main process.
;~	Params
;~		$bSuspended	- Create and suspend.
;~	Returns
;~		0+	- Index of sub process
;~		-1	- Reach the limit (over 10 sub processes) or error
func _MP_Fork($bSuspended = false)
	local $aRet = DllCall($__MP_hDll, 'int:cdecl', 'MP_Fork', 'int', $bSuspended)
	if not IsArray($aRet) or @error then return -1
	return $aRet[0]
endFunc

;~	Wait for a sub process exits.
;~	Params
;~		$iProc	- Sub process index (from _MP_Fork()).
func _MP_Wait($iProc)
	DllCall($__MP_hDll, 'none:cdecl', 'MP_Wait', 'int', $iProc)
endFunc

;~	Wait for all processes exit
func _MP_WaitAll()
	DllCall($__MP_hDll, 'none:cdecl', 'MP_WaitAll')
endFunc

;~	Resume a suspended sub process.
func _MP_Resume($iProc)
	DllCall($__MP_hDll, 'none:cdecl', 'MP_Resume', 'int', $iProc)
endFunc

;~	Suspend a sub process.
func _MP_Suspend($iProc)
	DllCall($__MP_hDll, 'none:cdecl', 'MP_Suspend', 'int', $iProc)
endFunc

;~	Get global shared data.
;~	The return value is an IDispatch object.
;~	The object properties can be accessed in any process.
func _MP_SharedData()
	local $aRet = DllCall($__MP_hDll, 'idispatch:cdecl', 'MP_SharedData')
	if not IsArray($aRet) or @error then return null
	return $aRet[0]
endFunc

;~	Send an integer message to sub process.
;~	This function must be called in main process.
;~	Params
;~		$iProc		- Sub process index
;~		$iMessage	- An integer message
func _MP_SendToSub($iProc, $iMessage)
	DllCall($__MP_hDll, 'none:cdecl', 'MP_SendToSub', 'int', $iProc, 'int', $iMessage)
endFunc

;~	Send an integer message to main process.
;~	This function must be called in sub process.
;~	Params
;~		$iMessage	- An integer message
func _MP_SendToMain($iMessage)
	DllCall($__MP_hDll, 'none:cdecl', 'MP_SendToMain', 'int', $iMessage)
endFunc

;~ 	Register a listenter to listen message is sent from main process.
;~ 	This function must be called in sub process.
;~	Params
;~		$fnListener	- A function has single integer argument.
;~ 	Returns
;~		A callback handler, you must call _MP_OffMain() to Remove it.
func _MP_OnMain($fnListener)
	local $hCallback = DllCallbackRegister($fnListener, 'none', 'int')
	DllCall($__MP_hDll, 'none:cdecl', 'MP_OnMain', 'ptr', DllCallbackGetPtr($hCallback))
	return $hCallback
endFunc

;~ 	Register a listenter to listen message is sent from sub process.
;~ 	This function must be called in main process.
;~	Params
;~		$fnListener	- A function has two integer arguments
;~ 	Returns
;~		A callback handler, you must call _MP_OffSub() to Remove it.
func _MP_OnSub($fnListener)
	local $hCallback = DllCallbackRegister($fnListener, 'none', 'int;int')
	DllCall($__MP_hDll, 'none:cdecl', 'MP_OnSub', 'ptr', DllCallbackGetPtr($hCallback))
	return $hCallback
endFunc

;~	Remove callback handler, which is returned by _MP_OnMain().
func _MP_OffMain($hCallback)
	DllCall($__MP_hDll, 'none:cdecl', 'MP_OffMain', 'ptr', DllCallbackGetPtr($hCallback))
	DllCallbackFree($hCallback)
endFunc

;~	Remove callback handler, which is returned by _MP_OnSub().
func _MP_OffSub($hCallback)
	DllCall($__MP_hDll, 'none:cdecl', 'MP_OffSub', 'ptr', DllCallbackGetPtr($hCallback))
	DllCallbackFree($hCallback)
endFunc