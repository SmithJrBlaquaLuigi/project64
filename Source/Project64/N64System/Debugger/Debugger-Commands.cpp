/****************************************************************************
*                                                                           *
* Project64 - A Nintendo 64 emulator.                                      *
* http://www.pj64-emu.com/                                                  *
* Copyright (C) 2012 Project64. All rights reserved.                        *
*                                                                           *
* License:                                                                  *
* GNU/GPLv2 http://www.gnu.org/licenses/gpl-2.0.html                        *
*                                                                           *
****************************************************************************/

#include "stdafx.h"
#include "DebuggerUI.h"

#include <Project64/UserInterface/resource.h>
#include <Project64-core/N64System/Mips/OpCodeName.h>
#include <Project64-core/N64System/Interpreter/InterpreterDBG.h>

static INT_PTR CALLBACK TabProcGPR(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK TabProcFPR(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

static bool registersUpdating = FALSE;

const int CDebugCommandsView::listLength = 36;

CDebugCommandsView::CDebugCommandsView(CDebuggerUI * debugger) :
CDebugDialog<CDebugCommandsView>(debugger)
{
	m_StartAddress = 0x80000000;
}

CDebugCommandsView::~CDebugCommandsView(void)
{

}

LRESULT	CDebugCommandsView::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CheckCPUType();

	// Setup address input

	m_EditAddress.Attach(GetDlgItem(IDC_CMD_ADDR));
	m_EditAddress.SetLimitText(8);

	char addrStr[9];
	sprintf(addrStr, "%08X", m_StartAddress);
	m_EditAddress.SetWindowText(addrStr);

	// Setup register tabs & inputs

	m_RegisterTabs.Attach(GetDlgItem(IDC_CMD_REGTABS));

	m_RegisterTabs.ResetTabs();

	m_TabGPR = m_RegisterTabs.AddTab("GPR", IDD_Debugger_GPR, TabProcGPR);
	m_TabFPR = m_RegisterTabs.AddTab("FPR", IDD_Debugger_FPR, TabProcFPR);

	static int gprEditIds[32] = {
		IDC_CMD_R0,  IDC_CMD_R1,  IDC_CMD_R2,  IDC_CMD_R3,
		IDC_CMD_R4,  IDC_CMD_R5,  IDC_CMD_R6,  IDC_CMD_R7,
		IDC_CMD_R8,  IDC_CMD_R9,  IDC_CMD_R10, IDC_CMD_R11,
		IDC_CMD_R12, IDC_CMD_R13, IDC_CMD_R14, IDC_CMD_R15,
		IDC_CMD_R16, IDC_CMD_R17, IDC_CMD_R18, IDC_CMD_R19,
		IDC_CMD_R20, IDC_CMD_R21, IDC_CMD_R22, IDC_CMD_R23,
		IDC_CMD_R24, IDC_CMD_R25, IDC_CMD_R26, IDC_CMD_R27,
		IDC_CMD_R28, IDC_CMD_R29, IDC_CMD_R30, IDC_CMD_R31
	};

	static int fprEditIds[32] = {
		IDC_CMD_F0,  IDC_CMD_F1,  IDC_CMD_F2,  IDC_CMD_F3,
		IDC_CMD_F4,  IDC_CMD_F5,  IDC_CMD_F6,  IDC_CMD_F7,
		IDC_CMD_F8,  IDC_CMD_F9,  IDC_CMD_F10, IDC_CMD_F11,
		IDC_CMD_F12, IDC_CMD_F13, IDC_CMD_F14, IDC_CMD_F15,
		IDC_CMD_F16, IDC_CMD_F17, IDC_CMD_F18, IDC_CMD_F19,
		IDC_CMD_F20, IDC_CMD_F21, IDC_CMD_F22, IDC_CMD_F23,
		IDC_CMD_F24, IDC_CMD_F25, IDC_CMD_F26, IDC_CMD_F27,
		IDC_CMD_F28, IDC_CMD_F29, IDC_CMD_F30, IDC_CMD_F31
	};

	for (int i = 0; i < 32; i++)
	{
		m_EditGPRegisters[i].Attach(m_TabGPR.GetDlgItem(gprEditIds[i]));
		m_EditGPRegisters[i].SetLimitText(8);

		m_EditFPRegisters[i].Attach(m_TabFPR.GetDlgItem(fprEditIds[i]));
		m_EditFPRegisters[i].SetLimitText(8);
	}

	m_EditGPRHI.Attach(m_TabGPR.GetDlgItem(IDC_CMD_RHI));
	m_EditGPRLO.Attach(m_TabGPR.GetDlgItem(IDC_CMD_RLO));

	RefreshRegisterEdits();

	// Setup breakpoint list

	m_BreakpointList.Attach(GetDlgItem(IDC_CMD_BPLIST));
	RefreshBreakpointList();

	// Setup command list

	m_CommandList.Attach(GetDlgItem(IDC_CMD_LIST));
	m_CommandList.AddColumn("Address", 0);
	m_CommandList.AddColumn("Command", 1);
	m_CommandList.AddColumn("Parameters", 2);
	m_CommandList.SetColumnWidth(0, 60);
	m_CommandList.SetColumnWidth(1, 60);
	m_CommandList.SetColumnWidth(2, 120);
	m_CommandList.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

	ShowAddress(m_StartAddress, TRUE);

	WindowCreated();
	return TRUE;
}

LRESULT CDebugCommandsView::OnDestroy(void)
{
	return 0;
}

void CDebugCommandsView::CheckCPUType()
{
	uint32_t cpuType = g_Settings->LoadDword(Game_CpuType);
	if (cpuType != CPU_TYPE::CPU_Interpreter)
	{
		MessageBox("Interpreter mode required", "Invalid CPU Type", MB_OK);
	}
}

void CDebugCommandsView::ShowAddress(DWORD address, BOOL top)
{
	if (address > 0x807FFFFC || address < 0x80000000)
	{
		return;
	}

	// if top == false, change start address only if address is out of view
	if (top == TRUE || address < m_StartAddress || address > m_StartAddress + (listLength-1) * 4)
	{
		m_StartAddress = address;
	}
	
	m_CommandList.SetRedraw(FALSE);
	m_CommandList.DeleteAllItems();

	char addrStr[9];

	for (int i = 0; i < listLength; i++)
	{	
		sprintf(addrStr, "%08X", m_StartAddress + i * 4);
		
		m_CommandList.AddItem(i, 0, addrStr);

		if (g_MMU == NULL)
		{
			m_CommandList.AddItem(i, 1, "???");
			m_CommandList.AddItem(i, 1, "???");
			continue;
		}
		
		OPCODE OpCode;
		g_MMU->LW_VAddr(m_StartAddress + i * 4, OpCode.Hex);

		char* command = (char*)R4300iOpcodeName(OpCode.Hex, m_StartAddress + i * 4);
		char* cmdName = strtok((char*)command, "\t");
		char* cmdArgs = strtok(NULL, "\t");

		m_CommandList.AddItem(i, 1, cmdName);
		m_CommandList.AddItem(i, 2, cmdArgs);
	}

	if (!top) // update registers when called via breakpoint
	{
		RefreshRegisterEdits();
	}
	
	m_CommandList.SetRedraw(TRUE);
	m_CommandList.RedrawWindow();
}

void CDebugCommandsView::RefreshRegisterEdits()
{
	registersUpdating = TRUE;
	if (g_Reg != NULL) {
		char regText[9];
		for (int i = 0; i < 32; i++)
		{
			// todo red labels when sign extension set
			sprintf(regText, "%08X", g_Reg->m_GPR[i].UW[0]);
			m_EditGPRegisters[i].SetWindowTextA(regText);

			sprintf(regText, "%08X", g_Reg->m_FPR[i].UW[0]);
			m_EditFPRegisters[i].SetWindowTextA(regText);
		}

		sprintf(regText, "%08X", g_Reg->m_HI.UW[0]);
		m_EditGPRHI.SetWindowTextA(regText);
		
		sprintf(regText, "%08X", g_Reg->m_LO.UW[0]);
		m_EditGPRLO.SetWindowTextA(regText);
	}
	registersUpdating = FALSE;
}

void CDebugCommandsView::RefreshBreakpointList()
{
	m_BreakpointList.ResetContent();
	char rowStr[16];
	for (int i = 0; i < CInterpreterDBG::m_nRBP; i++)
	{
		sprintf(rowStr, "R %08X", CInterpreterDBG::m_RBP[i]);
		int index = m_BreakpointList.AddString(rowStr);
		m_BreakpointList.SetItemData(index, CInterpreterDBG::m_RBP[i]);
	}
	for (int i = 0; i < CInterpreterDBG::m_nWBP; i++)
	{
		sprintf(rowStr, "W %08X", CInterpreterDBG::m_WBP[i]);
		int index = m_BreakpointList.AddString(rowStr);
		m_BreakpointList.SetItemData(index, CInterpreterDBG::m_WBP[i]);
	}
	for (int i = 0; i < CInterpreterDBG::m_nEBP; i++)
	{
		sprintf(rowStr, "E %08X", CInterpreterDBG::m_EBP[i]);
		int index = m_BreakpointList.AddString(rowStr);
		m_BreakpointList.SetItemData(index, CInterpreterDBG::m_EBP[i]);
	}
}

void CDebugCommandsView::RemoveSelectedBreakpoints()
{
	int selItemIndeces[256];
	int nSelItems = m_BreakpointList.GetSelItems(256, selItemIndeces);
	for (int i = 0; i < nSelItems; i++)
	{
		int index = selItemIndeces[i];
		char itemText[32];
		m_BreakpointList.GetText(index, itemText);
		uint32_t address = m_BreakpointList.GetItemData(index);
		switch (itemText[0])
		{
		case 'E':
			CInterpreterDBG::EBPRemove(address);
			break;
		case 'W':
			CInterpreterDBG::WBPRemove(address);
			break;
		case 'R':
			CInterpreterDBG::RBPRemove(address);
			break;
		}
	}
	RefreshBreakpointList();
}

LRESULT CDebugCommandsView::OnCustomDrawList(NMHDR* pNMHDR)
{
	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
	DWORD drawStage = pLVCD->nmcd.dwDrawStage;
	if (drawStage == CDDS_PREPAINT)
	{
		return CDRF_NOTIFYITEMDRAW;
	}
	if (drawStage == CDDS_ITEMPREPAINT)
	{
		return CDRF_NOTIFYSUBITEMDRAW;
	}
	if(drawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM))
	{
		DWORD nItem = pLVCD->nmcd.dwItemSpec;
		DWORD nSubItem = pLVCD->iSubItem;
		
		uint32_t address = m_StartAddress + (nItem * 4);
		uint32_t pc = (g_Reg != NULL) ? g_Reg->m_PROGRAM_COUNTER : 0;

		if (nSubItem == 0) // addr
		{
			if (CInterpreterDBG::EBPExists(address))
			{
				// breakpoint
				pLVCD->clrTextBk = RGB(0x44, 0x00, 0x00);
				pLVCD->clrText = (pc == address) ?
					RGB(0xFF, 0xFF, 0x00) : // breakpoint & current pc
					RGB(0xFF, 0xCC, 0xCC);
			}
			else if (pc == address)
			{
				// pc
				pLVCD->clrTextBk = RGB(0x88, 0x88, 0x88);
				pLVCD->clrText = RGB(0xFF, 0xFF, 0);
			}
			else
			{
				//default
				pLVCD->clrTextBk = RGB(0xEE, 0xEE, 0xEE);
				pLVCD->clrText = RGB(0x44, 0x44, 0x44);
			}
		}
		else if (nSubItem == 1 || nSubItem == 2) // cmd & args
		{
			OPCODE Opcode = OPCODE();
			if (g_MMU != NULL)
			{
				g_MMU->LW_VAddr(address, Opcode.Hex);
			}
			else
			{
				Opcode.Hex = 0x00000000;
			}
			
			if (pc == address)
			{
				//pc
				pLVCD->clrTextBk = RGB(0xFF, 0xFF, 0xAA);
				pLVCD->clrText = RGB(0x22, 0x22, 0);
			}
			else if (Opcode.op == R4300i_ADDIU && Opcode.rt == 29) // stack shift
			{
				if ((short)Opcode.immediate < 0) // alloc
				{
					// sky blue bg, dark blue fg
					pLVCD->clrTextBk = RGB(0xDD, 0xDD, 0xFF);
					pLVCD->clrText = RGB(0x00, 0x00, 0x44);
				}
				else // free
				{
					// salmon bg, dark red fg
					pLVCD->clrTextBk = RGB(0xFF, 0xDD, 0xDD);
					pLVCD->clrText = RGB(0x44, 0x00, 0x00);
				}
			}
			else if (Opcode.Hex == 0x00000000) // nop
			{
				// gray fg
				pLVCD->clrTextBk = RGB(0xFF, 0xFF, 0xFF);
				pLVCD->clrText = RGB(0x88, 0x88, 0x88);
			}
			else if (nSubItem == 1 && (Opcode.op == R4300i_J || Opcode.op == R4300i_JAL || (Opcode.op == 0 && Opcode.funct == R4300i_SPECIAL_JR)))
			{
				// jumps
				pLVCD->clrText = RGB(0x00, 0x88, 0x00);
				pLVCD->clrTextBk = RGB(0xFF, 0xFF, 0xFF);
			}
			else
			{
				pLVCD->clrTextBk = RGB(0xFF, 0xFF, 0xFF);
				pLVCD->clrText = RGB(0x00, 0x00, 0x00);
			}
		}
	}
	return CDRF_DODEFAULT;
}

LRESULT CDebugCommandsView::OnClicked(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	switch (wID)
	{
	case IDC_CMD_BTN_GO:
		CInterpreterDBG::StopDebugging();
		CInterpreterDBG::Resume();
		break;
	case IDC_CMD_BTN_STEP:
		CInterpreterDBG::KeepDebugging();
		CInterpreterDBG::Resume();
		break;
	case IDC_CMD_BTN_BPCLEAR:
		CInterpreterDBG::BPClear();
		RefreshBreakpointList();
		ShowAddress(m_StartAddress, TRUE);
		break;
	case IDC_CMD_ADDBP:
		m_AddBreakpointDlg.DoModal();
		RefreshBreakpointList();
		ShowAddress(m_StartAddress, TRUE);
		break;
	case IDC_CMD_RMBP:
		RemoveSelectedBreakpoints();
		ShowAddress(m_StartAddress, TRUE);
		break;
	case IDCANCEL:
		EndDialog(0);
		break;
	}
	return FALSE;
}

LRESULT CDebugCommandsView::OnMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	uint32_t newAddress = m_StartAddress - ((short)HIWORD(wParam) / WHEEL_DELTA) * 4;

	if (newAddress < 0x80000000 || newAddress > 0x807FFFFC) // todo mem size check
	{
		return TRUE;
	}

	m_StartAddress = newAddress;

	ShowAddress(m_StartAddress, TRUE);

	char addrStr[9];
	sprintf(addrStr, "%08X", m_StartAddress);
	
	m_EditAddress.SetWindowTextA(addrStr);

	return TRUE;
}

void CDebugCommandsView::GotoEnteredAddress()
{
	char text[9];

	m_EditAddress.GetWindowTextA(text, 9);

	DWORD address = strtoul(text, NULL, 16);
	address &= 0x007FFFFF;
	address |= 0x80000000;
	address = address - address % 4;
	ShowAddress(address, TRUE);
}

LRESULT CDebugCommandsView::OnAddrChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	GotoEnteredAddress();
	return 0;
}

LRESULT	CDebugCommandsView::OnListClicked(NMHDR* pNMHDR)
{
	NMITEMACTIVATE* pIA = reinterpret_cast<NMITEMACTIVATE*>(pNMHDR);
	int nItem = pIA->iItem;

	uint32_t address = m_StartAddress + nItem * 4;

	if (CInterpreterDBG::EBPExists(address))
	{
		CInterpreterDBG::EBPRemove(address);
	}
	else
	{
		CInterpreterDBG::EBPAdd(address);
	}

	// cancel blue highlight
	m_EditAddress.SetFocus();

	RefreshBreakpointList();

	return CDRF_DODEFAULT;
}

// Add breakpoint dialog

LRESULT CAddBreakpointDlg::OnClicked(WORD /*wNotifyCode*/, WORD wID, HWND, BOOL& /*bHandled*/)
{
	switch (wID)
	{
	case IDOK:
		{
			char addrStr[9];
			GetDlgItemText(IDC_ABP_ADDR, addrStr, 9);
			uint32_t address = strtoul(addrStr, NULL, 16);

			int read = ((CButton)GetDlgItem(IDC_ABP_CHK_READ)).GetCheck();
			int write = ((CButton)GetDlgItem(IDC_ABP_CHK_WRITE)).GetCheck();
			int exec = ((CButton)GetDlgItem(IDC_ABP_CHK_EXEC)).GetCheck();

			if (read)
			{
				CInterpreterDBG::RBPAdd(address);
			}
			if (write)
			{
				CInterpreterDBG::WBPAdd(address);
			}
			if (exec)
			{
				CInterpreterDBG::EBPAdd(address);
			}
			EndDialog(0);
			break;
		}
	case IDCANCEL:
		EndDialog(0);
		break;
	}
	return FALSE;
}

LRESULT CAddBreakpointDlg::OnDestroy(void)
{
	return 0;
}

void CRegisterTabs::ResetTabs()
{
	m_TabWindows.clear();
	m_TabIds.clear();
}

static INT_PTR CALLBACK TabProcGPR(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_INITDIALOG)
	{
		return TRUE;
	}
	if (registersUpdating || !CInterpreterDBG::m_Debugging)
	{
		return FALSE;
	}
	if (msg != WM_COMMAND || HIWORD(wParam) != EN_CHANGE)
	{
		return FALSE;
	}

	int controlId = LOWORD(wParam);

	int nReg = -1;

	switch (controlId)
	{
	//case IDC_CMD_R0:  nReg = 0; break;
	case IDC_CMD_R1:  nReg = 1; break;
	case IDC_CMD_R2:  nReg = 2; break;
	case IDC_CMD_R3:  nReg = 3; break;
	case IDC_CMD_R4:  nReg = 4; break;
	case IDC_CMD_R5:  nReg = 5; break;
	case IDC_CMD_R6:  nReg = 6; break;
	case IDC_CMD_R7:  nReg = 7; break;
	case IDC_CMD_R8:  nReg = 8; break;
	case IDC_CMD_R9:  nReg = 9; break;
	case IDC_CMD_R10: nReg = 10; break;
	case IDC_CMD_R11: nReg = 11; break;
	case IDC_CMD_R12: nReg = 12; break;
	case IDC_CMD_R13: nReg = 13; break;
	case IDC_CMD_R14: nReg = 14; break;
	case IDC_CMD_R15: nReg = 15; break;
	case IDC_CMD_R16: nReg = 16; break;
	case IDC_CMD_R17: nReg = 17; break;
	case IDC_CMD_R18: nReg = 18; break;
	case IDC_CMD_R19: nReg = 19; break;
	case IDC_CMD_R20: nReg = 20; break;
	case IDC_CMD_R21: nReg = 21; break;
	case IDC_CMD_R22: nReg = 22; break;
	case IDC_CMD_R23: nReg = 23; break;
	case IDC_CMD_R24: nReg = 24; break;
	case IDC_CMD_R25: nReg = 25; break;
	case IDC_CMD_R26: nReg = 26; break;
	case IDC_CMD_R27: nReg = 27; break;
	case IDC_CMD_R28: nReg = 28; break;
	case IDC_CMD_R29: nReg = 29; break;
	case IDC_CMD_R30: nReg = 30; break;
	case IDC_CMD_R31: nReg = 31; break;
	}
	
	if (g_Reg != NULL)
	{
		char regText[9];
		CWindow edit = GetDlgItem(hDlg, controlId);
		edit.GetWindowTextA(regText, 9);
		uint32_t value = strtoul(regText, NULL, 16);

		if (nReg > 0)
		{
			g_Reg->m_GPR[nReg].UW[0] = value;
		}
		else if (controlId == IDC_CMD_RHI)
		{
			g_Reg->m_HI.UW[0] = value;
		}
		else if (controlId == IDC_CMD_RLO)
		{
			g_Reg->m_LO.UW[0] = value;
		}
	}

	return FALSE;
}

static INT_PTR CALLBACK TabProcFPR(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_INITDIALOG)
	{
		return TRUE;
	}
	if (registersUpdating || !CInterpreterDBG::m_Debugging)
	{
		return FALSE;
	}
	if (msg != WM_COMMAND || HIWORD(wParam) != EN_CHANGE)
	{
		return FALSE;
	}
	
	int controlId = LOWORD(wParam);

	int nReg = -1;

	switch (controlId)
	{
	case IDC_CMD_F0:  nReg = 0; break;
	case IDC_CMD_F1:  nReg = 1; break;
	case IDC_CMD_F2:  nReg = 2; break;
	case IDC_CMD_F3:  nReg = 3; break;
	case IDC_CMD_F4:  nReg = 4; break;
	case IDC_CMD_F5:  nReg = 5; break;
	case IDC_CMD_F6:  nReg = 6; break;
	case IDC_CMD_F7:  nReg = 7; break;
	case IDC_CMD_F8:  nReg = 8; break;
	case IDC_CMD_F9:  nReg = 9; break;
	case IDC_CMD_F10: nReg = 10; break;
	case IDC_CMD_F11: nReg = 11; break;
	case IDC_CMD_F12: nReg = 12; break;
	case IDC_CMD_F13: nReg = 13; break;
	case IDC_CMD_F14: nReg = 14; break;
	case IDC_CMD_F15: nReg = 15; break;
	case IDC_CMD_F16: nReg = 16; break;
	case IDC_CMD_F17: nReg = 17; break;
	case IDC_CMD_F18: nReg = 18; break;
	case IDC_CMD_F19: nReg = 19; break;
	case IDC_CMD_F20: nReg = 20; break;
	case IDC_CMD_F21: nReg = 21; break;
	case IDC_CMD_F22: nReg = 22; break;
	case IDC_CMD_F23: nReg = 23; break;
	case IDC_CMD_F24: nReg = 24; break;
	case IDC_CMD_F25: nReg = 25; break;
	case IDC_CMD_F26: nReg = 26; break;
	case IDC_CMD_F27: nReg = 27; break;
	case IDC_CMD_F28: nReg = 28; break;
	case IDC_CMD_F29: nReg = 29; break;
	case IDC_CMD_F30: nReg = 30; break;
	case IDC_CMD_F31: nReg = 31; break;
	}

	if (nReg > 0 && g_Reg != NULL)
	{
		char regText[9];
		CWindow edit = GetDlgItem(hDlg, controlId);
		edit.GetWindowTextA(regText, 9);
		uint32_t value = strtoul(regText, NULL, 16);
		g_Reg->m_FPR[nReg].UW[0] = value;
	}

	return FALSE;
}

CWindow CRegisterTabs::AddTab(char* caption, int dialogId, DLGPROC dlgProc)
{
	AddItem(caption);
	
	CWindow parentWin = GetParent();
	CWindow tabWin = ::CreateDialog(NULL, MAKEINTRESOURCE(dialogId), parentWin, dlgProc);

	CRect pageRect;
	GetWindowRect(&pageRect);
	parentWin.ScreenToClient(&pageRect);
	AdjustRect(FALSE, &pageRect);

	::SetParent(tabWin, parentWin);

	::SetWindowPos(
		tabWin,
		m_hWnd,
		pageRect.left,
		pageRect.top,
		pageRect.Width(),
		pageRect.Height(),
		SWP_HIDEWINDOW
	);

	m_TabWindows.push_back(tabWin);
	m_TabIds.push_back(dialogId);
	
	int index = m_TabWindows.size() - 1;

	if (m_TabWindows.size() == 1)
	{
		ShowTab(0);
	}

	return tabWin;

}

void CRegisterTabs::ShowTab(int nPage)
{
	for (int i = 0; i < m_TabWindows.size(); i++)
	{
		::ShowWindow(m_TabWindows[i], SW_HIDE);
	}
	
	::SetWindowPos(
		m_TabWindows[nPage],
		m_hWnd,
		0, 0, 0, 0,
		SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE
	);
}

LRESULT CDebugCommandsView::OnRegisterTabChange(NMHDR* pNMHDR)
{
	int nPage = m_RegisterTabs.GetCurSel();
	m_RegisterTabs.ShowTab(nPage);
	return FALSE;
}