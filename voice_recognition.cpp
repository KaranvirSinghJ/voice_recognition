
#include <Windows.h>
#include <CommCtrl.h>
#include <vector>
#include <map>
#include "resource.h"
#include "pxcsensemanager.h"
#include "pxcspeechrecognition.h"
#include "voice_recognition.h"

#define ID_MODULEX   21000
#define ID_SOURCEX	 22000
#define ID_LANGUAGEX 23000

std::map<int, PXCAudioSource::DeviceInfo> g_devices;
std::map<int, pxcUID> g_modules;
extern PXCSession  *g_session;

/* Grammar & Vocabulary File Names */
extern pxcCHAR g_file[1024];
pxcCHAR v_file[1024] = { 0 };

pxcCHAR *LanguageToString(PXCSpeechRecognition::LanguageType language) {
	switch (language) {
	case PXCSpeechRecognition::LANGUAGE_US_ENGLISH:		return L"US English";
	case PXCSpeechRecognition::LANGUAGE_GB_ENGLISH:		return L"British English";
	case PXCSpeechRecognition::LANGUAGE_DE_GERMAN:		return L"Deutsch";
	case PXCSpeechRecognition::LANGUAGE_IT_ITALIAN:		return L"Italiano";
	case PXCSpeechRecognition::LANGUAGE_BR_PORTUGUESE:	return L"Português";
	case PXCSpeechRecognition::LANGUAGE_CN_CHINESE:		return L"中文";
	case PXCSpeechRecognition::LANGUAGE_FR_FRENCH:		return L"Français";
	case PXCSpeechRecognition::LANGUAGE_JP_JAPANESE:	return L"日本語";
	case PXCSpeechRecognition::LANGUAGE_US_SPANISH:		return L"US Español";
	case PXCSpeechRecognition::LANGUAGE_LA_SPANISH:		return L"LA Español";
	}
	return 0;
}

pxcCHAR *AlertToString(PXCSpeechRecognition::AlertType label) {
	switch (label) {
	case PXCSpeechRecognition::ALERT_SNR_LOW: return L"SNR_LOW";
	case PXCSpeechRecognition::ALERT_SPEECH_UNRECOGNIZABLE: return L"SPEECH_UNRECOGNIZABLE";
	case PXCSpeechRecognition::ALERT_VOLUME_HIGH: return L"VOLUME_HIGH";
	case PXCSpeechRecognition::ALERT_VOLUME_LOW: return L"VOLUME_LOW";
	case PXCSpeechRecognition::ALERT_SPEECH_BEGIN: return L"SPEECH_BEGIN";
	case PXCSpeechRecognition::ALERT_SPEECH_END: return L"SPEECH_END";
	case PXCSpeechRecognition::ALERT_RECOGNITION_ABORTED: return L"RECOGNITION_ABORTED";
	case PXCSpeechRecognition::ALERT_RECOGNITION_END: return L"RECOGNITION_END";
	}
	return L"Unknown";
}

pxcCHAR *NewCommand(void) {
	return L"[Enter New Command]";
}

void PrintStatus(HWND hWnd, pxcCHAR *string) {

	HWND hTreeView = GetDlgItem(hWnd, IDC_STATUS);
	TVINSERTSTRUCT tvis;
	memset(&tvis, 0, sizeof(tvis));
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_TEXT;
	tvis.item.pszText = string;
	tvis.item.cchTextMax = (int)wcslen(string);
	TreeView_EnsureVisible(hTreeView, TreeView_InsertItem(hTreeView, &tvis));
}

void PrintConsole(HWND hWnd, pxcCHAR *string) {

	HWND hTreeView = GetDlgItem(hWnd, IDC_CONSOLE2);
	TVINSERTSTRUCT tvis;
	memset(&tvis, 0, sizeof(tvis));
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_TEXT;
	tvis.item.pszText = string;
	tvis.item.cchTextMax = (int)wcslen(string);
	TreeView_EnsureVisible(hTreeView, TreeView_InsertItem(hTreeView, &tvis));
}

PXCAudioSource::DeviceInfo GetAudioSource(HWND hWnd) {

	HMENU menu1 = GetSubMenu(GetMenu(hWnd), 0);	 ID_SOURCE
?	int item = GetCheckedItem(menu1);
	MENUITEMINFO minfo = {};
	minfo.cbSize = sizeof(MENUITEMINFO);
?	GetMenuItemInfo(menu1, item, MF_BYPOSITION, &minfo);
	return g_devices[minfo.wID];
}

std::vector<std::wstring> GetCommands(HWND hWnd) {
	HWND hwndConsole = GetDlgItem(hWnd, IDC_CONSOLE2);
	TVITEM tvi;
	tvi.mask = TVIF_TEXT;
	pxcCHAR line[256];
	tvi.pszText = line;
	tvi.cchTextMax = sizeof(line) / sizeof(pxcCHAR);
	std::vector<std::wstring> cmds;
	cmds.push_back(L"one");
	cmds.push_back(L"two");

	for (HTREEITEM item = TreeView_GetRoot(hwndConsole); item; item = TreeView_GetNextSibling(hwndConsole, item)) {
		tvi.hItem = item;
		TreeView_GetItem(hwndConsole, &tvi);
		TrimScore(line, tvi.cchTextMax);
		if (line[0]) cmds.push_back(std::wstring(line));
	}
	return cmds;
}

pxcUID GetModule(HWND hWnd) {
	HMENU menu1 = GetSubMenu(GetMenu(hWnd), 1);	 ID_MODULE
?	int item = GetCheckedItem(menu1);
	MENUITEMINFO minfo = {};
	minfo.cbSize = sizeof(MENUITEMINFO);
?	GetMenuItemInfo(menu1, item, MF_BYPOSITION, &minfo);
	return g_modules[minfo.wID];
}

int  GetLanguage(HWND hWnd) {
	HMENU menu1 = GetSubMenu(GetMenu(hWnd), 2);	 ID_LANGUAGE
?	return GetCheckedItem(menu1);
	return 1;
}

void FillCommandListConsole(HWND hWnd, pxcCHAR *filename) {
	FILE *gfile;
	_wfopen_s(&gfile, filename, L"rb");
	if (!gfile)
		return;

	TreeView_DeleteAllItems(GetDlgItem(hWnd, IDC_STATUS));
	TreeView_DeleteAllItems(GetDlgItem(hWnd, IDC_CONSOLE2));

	HWND hwndConsole = GetDlgItem(hWnd, IDC_CONSOLE2);
	PromptNewCommand(hwndConsole);

	char line[1024];
	memset(line, 0, 1024);
	pxcCHAR wline[1024];
	memset(wline, 0, 1024);
	for (int linenum = 0; !feof(gfile); linenum++)
	{
		if (!fgets(line, 1024, gfile))
			break;
		while (isspace(line[strlen(line) - 1]))
			line[strlen(line) - 1] = '\0';
		MultiByteToWideChar(CP_UTF8, 0, line, -1, wline, 1024);

		TVINSERTSTRUCT tvis;
		tvis.item.mask = TVIF_TEXT;
		tvis.item.pszText = wline;
		tvis.item.cchTextMax = sizeof(wline) / sizeof(pxcCHAR);
		tvis.hParent = 0;
		tvis.hInsertAfter = TVI_LAST;
		TreeView_InsertItem(hwndConsole, &tvis);
	}
}

static void GetGrammarFile(void) {
	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = L"JSGF files\0*.jsgf\0LIST files\0*.list\0All Files\0*.*\0\0";
	ofn.lpstrFile = g_file; g_file[0] = 0;
	ofn.nMaxFile = sizeof(g_file) / sizeof(pxcCHAR);
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
	if (!GetOpenFileName(&ofn)) g_file[0] = 0;
}

static void PopulateSource() {
	DeleteMenu(menu, 0, MF_BYPOSITION);
	HMENU menu1 = CreatePopupMenu();

	g_devices.clear();
	PXCAudioSource *source = g_session->CreateAudioSource();
	if (source) {
		source->ScanDevices();

		for (int i = 0, k = ID_SOURCEX;; i++) {
			PXCAudioSource::DeviceInfo dinfo = {};
			if (source->QueryDeviceInfo(i, &dinfo)<PXC_STATUS_NO_ERROR) break;
			AppendMenu(menu1, MF_STRING, k, dinfo.name);
			g_devices[k] = dinfo;
			k++;
		}

		source->Release();
	}

	CheckMenuRadioItem(menu1, 0, GetMenuItemCount(menu1), 0, MF_BYPOSITION);
	InsertMenu(menu, 0, MF_BYPOSITION | MF_POPUP, (UINT_PTR)menu1, L"Source");
}

static void PopulateModules() {
	DeleteMenu(menu, 1, MF_BYPOSITION);

	g_modules.clear();
	PXCSession::ImplDesc desc = {}, desc1;
	desc.cuids[0] = PXCSpeechRecognition::CUID;
	HMENU menu1 = CreatePopupMenu();
	int i;
	for (i = 0;; i++) {
		if (g_session->QueryImpl(&desc, i, &desc1)<PXC_STATUS_NO_ERROR) break;
		AppendMenu(menu1, MF_STRING, ID_MODULEX + i, desc1.friendlyName);
		g_modules[ID_MODULEX + i] = desc1.iuid;
	}
	CheckMenuRadioItem(menu1, 0, i, 0, MF_BYPOSITION);
	InsertMenu(menu, 1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)menu1, L"Module");
}

static void PopulateLanguage() {

	DeleteMenu(menu, 2, MF_BYPOSITION);

	HMENU menu1 = CreatePopupMenu();
	PXCSession::ImplDesc desc, desc1;
	memset(&desc, 0, sizeof(desc));
	desc.cuids[0] = PXCSpeechRecognition::CUID;
?	if (g_session->QueryImpl(&desc, GetCheckedItem(GetSubMenu(menu, 1)) /*ID_MODULE*/, &desc1) >= PXC_STATUS_NO_ERROR) {
		PXCSpeechRecognition *vrec;
		if (g_session->CreateImpl<PXCSpeechRecognition>(&desc1, &vrec) >= PXC_STATUS_NO_ERROR) {
			for (int k = 0;; k++) {
				PXCSpeechRecognition::ProfileInfo pinfo;
				if (vrec->QueryProfile(k, &pinfo)<PXC_STATUS_NO_ERROR) break;
				AppendMenu(menu1, MF_STRING, ID_LANGUAGEX + k, LanguageToString(pinfo.language));
			}
			vrec->Release();
		}
?	}
	CheckMenuRadioItem(menu1, 0, GetMenuItemCount(menu1), 0, MF_BYPOSITION);
	InsertMenu(menu, 2, MF_BYPOSITION | MF_POPUP, (UINT_PTR)menu1, L"Language");
}

static void TrimSpace(pxcCHAR *line, int nchars) {
	while (line[0] == L' ') wcscpy_s(line, nchars, line + 1);
	for (int i = 0; i<nchars; i++) {
		if (line[i]) continue;
		for (int j = i - 1; j >= 0; j--) {
			if (line[j] != L' ') break;
			line[j] = 0;
		}
		break;
	}
}

static void TrimScore(pxcCHAR *line, int nchars) {
	pxcCHAR *p = wcschr(line, L'[');
	if (p) *p = 0;
	TrimSpace(line, nchars);
}

void ClearScores(HWND hWnd) {
	HWND hwndConsole = GetDlgItem(hWnd, IDC_CONSOLE2);
	TVINSERTSTRUCT tvis;
	tvis.item.mask = TVIF_TEXT;
	pxcCHAR line[256] = { 0 };
	tvis.item.pszText = line;
	tvis.item.cchTextMax = sizeof(line) / sizeof(pxcCHAR);
	for (HTREEITEM item = TreeView_GetRoot(hwndConsole); item; item = TreeView_GetNextSibling(hwndConsole, item)) {
		tvis.item.hItem = item;
		TreeView_GetItem(hwndConsole, &tvis.item);
		TrimScore(line, tvis.item.cchTextMax);
		if (line[0]) TreeView_SetItem(hwndConsole, &tvis.item);
	}
}

void SetScore(HWND hWnd, int label, pxcI32 confidence) {
	HWND hwndConsole = GetDlgItem(hWnd, IDC_CONSOLE2);
	TVITEM tvi;
	tvi.mask = TVIF_TEXT;
	pxcCHAR line[256] = { 0 };
	tvi.pszText = line;
	tvi.cchTextMax = sizeof(line) / sizeof(pxcCHAR);
	for (HTREEITEM item = TreeView_GetRoot(hwndConsole); item; item = TreeView_GetNextSibling(hwndConsole, item)) {
		tvi.hItem = item;
		TreeView_GetItem(hwndConsole, &tvi);
		TrimScore(line, tvi.cchTextMax);
		if (!line[0]) continue;
		if (label--) continue;
		swprintf_s(line, tvi.cchTextMax, L"%s [%d%%]", line, confidence);
		TreeView_SetItem(hwndConsole, &tvi);
		break;
	}
}

/* New API */
 bool  SetVoiceCommands(std::vector<std::wstring> &cmds);
 bool  SetGrammarFromFile(HWND hWnd, pxcCHAR* GrammarFilename);
 bool  GetGrammarCompileErrors(pxcCHAR** grammarCompileErrors);
 bool  SetVocabularyFromFile(pxcCHAR* VocFilename);

class MyHandler: public PXCSpeechRecognition::Handler {
public:

    MyHandler(HWND hWnd):m_hWnd(hWnd) {}
	virtual ~MyHandler(){}

	virtual void PXCAPI OnRecognition(const PXCSpeechRecognition::RecognitionData *data) {
		if (data->scores[0].label<0) {
			PrintConsole(m_hWnd,(pxcCHAR*)data->scores[0].sentence);
			if (data->scores[0].tags[0])
				PrintConsole(m_hWnd,(pxcCHAR*)data->scores[0].tags);
		} else {

			ClearScores(m_hWnd);
			for (int i=0;i<sizeof(data->scores)/sizeof(data->scores[0]);i++) {
				if (data->scores[i].label < 0 || data->scores[i].confidence == 0) continue;
				SetScore(m_hWnd,data->scores[i].label,data->scores[i].confidence);
			}
			if (data->scores[0].tags[0])
				PrintStatus(m_hWnd,(pxcCHAR*)data->scores[0].tags);
		}
	}

	virtual void PXCAPI OnAlert(const PXCSpeechRecognition::AlertData *data) {
		if(data->label == PXCSpeechRecognition::AlertType::ALERT_SPEECH_UNRECOGNIZABLE)
		{
			ClearScores(m_hWnd);
		}
		PrintStatus(m_hWnd,AlertToString(data->label));
	}

protected:

	HWND m_hWnd;
};

static volatile bool g_stop=false;
static volatile bool g_stopped=true;
static PXCSpeechRecognition* g_vrec=0;
static PXCAudioSource* g_source=0;
static MyHandler* g_handler=0;

static void CleanUp(void) {
	if (g_handler) delete g_handler, g_handler=0;
	if (g_vrec) g_vrec->Release(), g_vrec=0;
	if (g_source) g_source->Release(), g_source=0;
	g_stopped=true;
}

DWORD WINAPI RecThread(HWND hWnd) {
	g_stopped=false;

	/* Create an Audio Source */
	g_source=g_session->CreateAudioSource();
	if (!g_source) {
		PrintStatus(hWnd,L"Failed to create the audio source");
		CleanUp();
		return 0;
	}

	/* Set Audio Source */
	PXCAudioSource::DeviceInfo dinfo=GetAudioSource(hWnd);
	pxcStatus sts=g_source->SetDevice(&dinfo);
	if (sts<PXC_STATUS_NO_ERROR) {
		PrintStatus(hWnd,L"Failed to set the audio source");
		CleanUp();
		return 0;
	}

	/* Set Module */
	PXCSession::ImplDesc mdesc={};
	mdesc.iuid=GetModule(hWnd);
	sts=g_session->CreateImpl<PXCSpeechRecognition>(&g_vrec);
	if (sts<PXC_STATUS_NO_ERROR) {
		PrintStatus(hWnd,L"Failed to create the module instance");
		CleanUp();
		return 0;
	}

	/* Set configuration according to user-selected language */
	PXCSpeechRecognition::ProfileInfo pinfo;
	int language_idx = GetLanguage(hWnd);
	g_vrec->QueryProfile(language_idx, &pinfo);
	sts = g_vrec->SetProfile(&pinfo);
	if (sts < PXC_STATUS_NO_ERROR)
	{
		PrintStatus(hWnd,L"Failed to set language");
		CleanUp();
		return 0;
	}

		/* Set Command/Control or Dictation */
		if (IsCommandControl(hWnd)) {
		if (true) {  suppose it is command control
			 Connamd/Control mode
			std::vector<std::wstring> cmds=GetCommands(hWnd);

			if (g_file && g_file[0] != '\0') {
				wchar_t *fileext1 = wcsrchr(g_file, L'.');
				const wchar_t *fileext = ++fileext1;
				if (wcscmp(fileext,L"list") == 0 || wcscmp(fileext,L"txt") == 0) {
					 voice commands loaded from file: put into gui
					FillCommandListConsole(hWnd,g_file);
					cmds=GetCommands(hWnd);
					if (cmds.empty())
						PrintStatus(hWnd,L"Command List Load Errors");
				}

				 input Command/Control grammar file available, use it
				if (!SetGrammarFromFile(hWnd, g_file)) {
					PrintStatus(hWnd,L"Can not set Grammar From File.");
					CleanUp();
					return -1;
				};

			} else if (!cmds.empty()) {
				 voice commands available, use them
				if (!SetVoiceCommands(cmds)) {
					PrintStatus(hWnd,L"Can not set Command List.");
					CleanUp();
					return -1;
				};

			} else {
				 revert to dictation mode
				PrintStatus(hWnd,L"No Command List or Grammar. Using dictation instead.");
				if (v_file && v_file[0] != '\0')  SetVocabularyFromFile(v_file);
				if (g_vrec->SetDictation() != PXC_STATUS_NO_ERROR) {
					PrintStatus(hWnd,L"Can not set Dictation mode.");
					CleanUp();
					return -1;
				};
			}

		} else {
			 Dictation mode
			if (v_file && v_file[0] != '\0')  SetVocabularyFromFile(v_file);
			if (g_vrec->SetDictation() != PXC_STATUS_NO_ERROR) {
					PrintStatus(hWnd,L"Can not set Dictation mode.");
					CleanUp();
					return -1;
				};
		}

	/* Start Recognition */
	PrintStatus(hWnd,L"Start Recognition");
	g_handler=new MyHandler(hWnd);
	sts=g_vrec->StartRec(g_source, g_handler);
	if (sts<PXC_STATUS_NO_ERROR) {
		PrintStatus(hWnd,L"Failed to start recognition");
		CleanUp();
		return 0;
	}

	PrintStatus(hWnd,L"Recognition Started");
	return 0;
}

void Start(HWND hwndDlg) {
	if (!g_stopped) return;

	PopulateModules();
	PopulateSource();
	CheckMenuRadioItem(GetSubMenu(menu, 3), 0, 1, 1, MF_BYPOSITION);
	PopulateLanguage();

	g_stop=false;
	RecThread(hwndDlg);
	Sleep(5);
}

bool Stop(HWND hwndDlg) {

	if (g_vrec) g_vrec->StopRec();
	g_stopped=true;
	CleanUp();
	PrintStatus(hwndDlg,L"Recognition Finished");
	return g_stopped;
}

bool IsRunning(void) {
	return !g_stopped;
}

bool SetVoiceCommands(std::vector<std::wstring> &cmds) {

    pxcUID grammar = 1;
	pxcEnum lbl=0;
	pxcCHAR ** _cmds = new pxcCHAR *[cmds.size()];
    for (std::vector<std::wstring >::iterator itr=cmds.begin();itr!=cmds.end();itr++,lbl++) {
		_cmds[lbl] = (pxcCHAR *)itr->c_str();
    }

	pxcStatus sts;

	sts = g_vrec->BuildGrammarFromStringList(grammar,_cmds,NULL,cmds.size());
	if (sts < PXC_STATUS_NO_ERROR) return false;

    sts = g_vrec->SetGrammar(grammar);
	if (sts < PXC_STATUS_NO_ERROR) return false;

    return true;
}

bool SetGrammarFromFile(HWND hWnd, pxcCHAR* GrammarFilename) {
    if (!g_vrec) {
        return true; / not setting stackable profile now
    }

    pxcUID grammar = 1;

	pxcStatus sts = g_vrec->BuildGrammarFromFile(grammar, PXCSpeechRecognition::GFT_NONE, GrammarFilename);
	if (sts < PXC_STATUS_NO_ERROR) {
		PrintStatus(hWnd,L"Grammar Compile Errors:");
		PrintStatus(hWnd,(pxcCHAR *) g_vrec->GetGrammarCompileErrors(grammar)); SM Is itapplicable?
		return false;
	}

	sts = g_vrec->SetGrammar(grammar);
	if (sts < PXC_STATUS_NO_ERROR) return false;


    return true;
}

bool SetVocabularyFromFile(pxcCHAR* VocFilename) {
    if (!g_vrec) {
        return true; / not setting stackable profile now
    }

	pxcStatus sts = g_vrec->AddVocabToDictation(PXCSpeechRecognition::VFT_LIST, VocFilename);
	if (sts < PXC_STATUS_NO_ERROR) return false;

    return true;
}
