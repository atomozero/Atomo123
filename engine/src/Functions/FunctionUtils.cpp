/*
	Copyright 1996, 1997, 1998, 2000
	        Hekkelman Programmatuur B.V.  All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright notice,
	   this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.
	3. All advertising materials mentioning features or use of this software
	   must display the following acknowledgement:
	   
	    This product includes software developed by Hekkelman Programmatuur B.V.
	
	4. The name of Hekkelman Programmatuur B.V. may not be used to endorse or
	   promote products derived from this software without specific prior
	   written permission.
	
	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
	AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
	OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
	WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
	OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/
/*
	FunctionUtils.c

	Copyright 1997, Hekkelman Programmatuur
	Portions Copyright 2026 Andrea Bernardi

	Part of Sum-It for the BeBox version 1.1.

*/

#ifndef   CONTAINER_H
#include "Container.h"
#endif

#ifndef   FUNCTIONUTILS_H
#include "FunctionUtils.h"
#endif

#ifndef   MYERROR_H
#include "MyError.h"
#endif

#ifndef   FUNCTIONS_H
#include "Functions.h"
#endif

#ifndef   UTILS_H
#include "Utils.h"
#endif

#ifndef   RESOURCEMANAGER_H
#include "ResourceManager.h"
#endif

#ifndef   STRINGTABLE_H
#include "StringTable.h"
#endif

#ifndef   GLOBALS_H
#include "Globals.h"
#endif

#include <AppFileInfo.h>
#include <Application.h>
#include <Path.h>
#include <Roster.h>
#include <kernel/image.h>

#include <cstdlib>
#include <unistd.h>
#include <dirent.h>


Function	*gFuncs;
FuncRec		*gFuncArrayByNr, *gFuncArrayByName;
long		gFuncCount;
const char		**gFuncDescriptions, **gFuncPasteStrings;

const type_code kFuncResType = 'Func';

void LoadPlugIns();
void SetupDefaultFuncs();
void SortByName();
int Compare(const void *e1, const void *e2);

void InitFunctions()
{
	gFuncs = (Function*)MALLOC(kFunctionCount * sizeof(Function));
	FailNil(gFuncs);

	const void * h;
	h = gResourceManager.LoadResource(kFuncResType, 128);
	if (!h) THROW((errResNotFound));
	gFuncArrayByNr = (FuncRec *)h;
	gFuncCount = gResourceManager.HandleSize(h) / sizeof(FuncRec);

	gFuncDescriptions = (const char**)MALLOC(gFuncCount * sizeof(char*));
	FailNil(gFuncDescriptions);

	int i;	
	for (i = 0; i < gFuncCount; i++)
		gFuncDescriptions[i] = GetIndString(8, i);
	
	gFuncPasteStrings = (const char**)MALLOC(gFuncCount * sizeof(char*));
	FailNil(gFuncPasteStrings);

	for (i = 0; i < gFuncCount; i++)
		gFuncPasteStrings[i] = GetIndString(7, i);
	
	LoadPlugIns();
	SortByName();

	SetupDefaultFuncs();
} /* InitFunctions */

void EnsureFunctionsInitialized()
{
	if (gFuncCount > 0 || be_app == NULL)
		return;

	// be_app riflette sempre il VERO processo in esecuzione (Atomo123,
	// che ha la risorsa 'Func' allegata al proprio eseguibile) anche
	// quando questo codice gira dentro un add-on caricato in-process
	// (un translator .so, con la propria copia STATICA di libengine.a
	// e quindi una propria istanza SEPARATA di gFuncCount/
	// gFuncArrayByNr/ecc., mai popolata da App::ReadyToRun) -- stesso
	// principio e stesso codice gia' in uso in Excel.cpp per lo stesso
	// identico bug sul lato import XLS legacy, consolidato qui perche'
	// XLSX/ODS avevano bisogno esattamente dello stesso fix.
	app_info info;
	if (be_app->GetAppInfo(&info) != B_OK)
		return;

	BPath path(&info.ref);
	if (path.InitCheck() != B_OK)
		return;

	// gAppName va impostato PRIMA di InitFunctions(): la usa
	// LoadPlugIns() per cercare un'eventuale cartella "Functions/"
	// accanto al binario -- omettendola lascerebbe gAppName al suo
	// BPath vuoto predefinito, il cui Path() restituisce NULL (bug
	// reale gia' scoperto una volta per Excel.cpp: comportamento
	// indefinito invece dell'eccezione pulita attesa).
	gAppName = path;
	gResourceManager.SetTo(&path);
	try
	{
		InitFunctions();
	}
	catch (CErr &e)
	{
		// Stesso comportamento di App::ReadyToRun: un fallimento qui
		// (binario senza la risorsa 'Func', percorso inatteso) non
		// deve impedire l'importazione -- si degrada a nessuna
		// funzione con nome disponibile, non a un crash.
	}
} /* EnsureFunctionsInitialized */

int Compare(const void *e1, const void *e2)
{
	FuncRec *a, *b;
	a = (FuncRec *)e1;
	b = (FuncRec *)e2;
	
	int diff = strcmp(a->funcName, b->funcName);
	return diff;
} /* Compare */

void SortByName()
{
	gFuncArrayByName = (FuncRec *)MALLOC(gFuncCount * sizeof(FuncRec));
	FailNil(gFuncArrayByName);
	memcpy(gFuncArrayByName, gFuncArrayByNr, sizeof(FuncRec) * gFuncCount);
	qsort(gFuncArrayByName, gFuncCount, sizeof(FuncRec), Compare);
	for (int i = 1; i < gFuncCount; i++)
		if (strcmp(gFuncArrayByName[i - 1].funcName,
				gFuncArrayByName[i].funcName) == 0)
		THROW((errDuplicateFunction, gFuncArrayByName[i].funcName));
} /* SortByName */

void SetupDefaultFuncs()
{
	gFuncs[kABSFuncNr] = ABSFunction;
	gFuncs[kACOSFuncNr] = ACOSFunction;
	gFuncs[kANNUITYFuncNr] = ANNUITYFunction;
	gFuncs[kASCFuncNr] = ASCFunction;
	gFuncs[kASINFuncNr] = ASINFunction;
	gFuncs[kATANFuncNr] = ATANFunction;
	gFuncs[kAVGFuncNr] = AVGFunction;
	gFuncs[kCEILINGFuncNr] = CEILINGFunction;
	gFuncs[kCELLFuncNr] = CELLFunction;
	gFuncs[kCHOOSEFuncNr] = CHOOSEFunction;
	gFuncs[kCHRFuncNr] = CHRFunction;
	gFuncs[kCOLUMNFuncNr] = COLUMNFunction;
	gFuncs[kCOMPOUNDFuncNr] = COMPOUNDFunction;
	gFuncs[kCOSFuncNr] = COSFunction;
	gFuncs[kCOTFuncNr] = COTFunction;
	gFuncs[kCOUNTFuncNr] = COUNTFunction;
	gFuncs[kDATEFuncNr] = DATEFunction;
	gFuncs[kDAYFuncNr] = DAYFunction;
	gFuncs[kDBFuncNr] = DBFunction;
	gFuncs[kDOCUMENTFuncNr] = DOCUMENTFunction;
	gFuncs[kDOWFuncNr] = DOWFunction;
	gFuncs[kERRFuncNr] = ERRFunction;
	gFuncs[kERRORFuncNr] = ERRORFunction;
	gFuncs[kEXPFuncNr] = EXPFunction;
	gFuncs[kFALSEFuncNr] = FALSEFunction;
	gFuncs[kTRUEFuncNr] = TRUEFunction;
	gFuncs[kFLOORFuncNr] = FLOORFunction;
	gFuncs[kFRACFuncNr] = FRACFunction;
	gFuncs[kFVFuncNr] = FVFunction;
	gFuncs[kHINDEXFuncNr] = HINDEXFunction;
	gFuncs[kHLOOKUPFuncNr] = HLOOKUPFunction;
	gFuncs[kHOURFuncNr] = HOURFunction;
	gFuncs[kIFFuncNr] = IFFunction;
	gFuncs[kIFERRFuncNr] = IFERRFunction;
	gFuncs[kINTFuncNr] = INTFunction;
	gFuncs[kIRRFuncNr] = IRRFunction;
	gFuncs[kISNULLFuncNr] = ISNULLFunction;
	gFuncs[kISNUMFuncNr] = ISNUMFunction;
	gFuncs[kISTEXTFuncNr] = ISTEXTFunction;
	gFuncs[kLEFTFuncNr] = LEFTFunction;
	gFuncs[kLENGTHFuncNr] = LENGTHFunction;
	gFuncs[kLNFuncNr] = LNFunction;
	gFuncs[kLOGFuncNr] = LOGFunction;
	gFuncs[kMAXFuncNr] = MAXFunction;
	gFuncs[kMIDFuncNr] = MIDFunction;
	gFuncs[kMINFuncNr] = MINFunction;
	gFuncs[kMINUTEFuncNr] = MINUTEFunction;
	gFuncs[kMODFuncNr] = MODFunction;
	gFuncs[kMONTHFuncNr] = MONTHFunction;
	gFuncs[kNAFuncNr] = NAFunction;
	gFuncs[kNCOLSFuncNr] = NCOLSFunction;
	gFuncs[kNOWFuncNr] = NOWFunction;
	gFuncs[kNPVFuncNr] = NPVFunction;
	gFuncs[kNROWSFuncNr] = NROWSFunction;
	gFuncs[kNUM2CFuncNr] = NUM2CFunction;
	gFuncs[kNUMPAGESFuncNr] = NUMPAGESFunction;
	gFuncs[kOFFSETFuncNr] = OFFSETFunction;
	gFuncs[kPAGEFuncNr] = PAGEFunction;
	gFuncs[kPIFuncNr] = PIFunction;
	gFuncs[kPMTFuncNr] = PMTFunction;
	gFuncs[kPVFuncNr] = PVFunction;
	gFuncs[kRANDOMFuncNr] = RANDOMFunction;
	gFuncs[kRIGHTFuncNr] = RIGHTFunction;
	gFuncs[kROUNDFuncNr] = ROUNDFunction;
	gFuncs[kROWFuncNr] = ROWFunction;
	gFuncs[kSECONDFuncNr] = SECONDFunction;
	gFuncs[kSIGNFuncNr] = SIGNFunction;
	gFuncs[kSINFuncNr] = SINFunction;
	gFuncs[kSLFuncNr] = SLFunction;
	gFuncs[kSOYDFuncNr] = SOYDFunction;
	gFuncs[kSQRTFuncNr] = SQRTFunction;
	gFuncs[kSTDDEVFuncNr] = STDDEVFunction;
	gFuncs[kSUMFuncNr] = SUMFunction;
	gFuncs[kTANFuncNr] = TANFunction;
	gFuncs[kTIMEFuncNr] = TIMEFunction;
	gFuncs[kTIME2CFuncNr] = TIME2CFunction;
	gFuncs[kVARIANCEFuncNr] = VARIANCEFunction;
	gFuncs[kVINDEXFuncNr] = VINDEXFunction;
	gFuncs[kVLOOKUPFuncNr] = VLOOKUPFunction;
	gFuncs[kYEARFuncNr] = YEARFunction;
	gFuncs[kORFuncNr] = ORFunction;
	gFuncs[kANDFuncNr] = ANDFunction;
	gFuncs[kPOWERFuncNr] = POWERFunction;
	gFuncs[kDEC2HEXFuncNr] = DEC2HEXFunction;
	gFuncs[kATAN2FuncNr] = ATAN2Function;
	gFuncs[kCOUNTAFuncNr] = COUNTAFunction;
	gFuncs[kSUMIFFuncNr] = SUMIFFunction;
	gFuncs[kCOUNTIFFuncNr] = COUNTIFFunction;
	gFuncs[kCOUNTIFSFuncNr] = COUNTIFSFunction;
	gFuncs[kROUNDUPFuncNr] = ROUNDUPFunction;
	gFuncs[kROUNDDOWNFuncNr] = ROUNDDOWNFunction;
	gFuncs[kTEXTFuncNr] = TEXTFunction;
	gFuncs[kAVERAGEIFFuncNr] = AVERAGEIFFunction;
	gFuncs[kTRIMFuncNr] = TRIMFunction;
	gFuncs[kUPPERFuncNr] = UPPERFunction;
	gFuncs[kLOWERFuncNr] = LOWERFunction;
	gFuncs[kPROPERFuncNr] = PROPERFunction;
	gFuncs[kFINDFuncNr] = FINDFunction;
	gFuncs[kSEARCHFuncNr] = SEARCHFunction;
	gFuncs[kCONCATFuncNr] = CONCATFunction;
	gFuncs[kMEDIANFuncNr] = MEDIANFunction;
	gFuncs[kMODEFuncNr] = MODEFunction;
	// Stessa IFERRFunction di kIFERRFuncNr sopra, non una nuova
	// implementazione: "IFERROR" e' solo il nome standard Excel che un
	// file XLSX vero usa davvero (vedi il commento sulla voce
	// "IFERROR" in funcs_by_nr.r).
	gFuncs[kIFERRORFuncNr] = IFERRFunction;
	gFuncs[kINDEXFuncNr] = INDEXFunction;
	gFuncs[kMATCHFuncNr] = MATCHFunction;
	gFuncs[kXLOOKUPFuncNr] = XLOOKUPFunction;
	gFuncs[kIFSFuncNr] = IFSFunction;
	gFuncs[kNOTFuncNr] = NOTFunction;
	gFuncs[kXORFuncNr] = XORFunction;
	gFuncs[kSWITCHFuncNr] = SWITCHFunction;
	gFuncs[kIFNAFuncNr] = IFNAFunction;
	gFuncs[kISBLANKFuncNr] = ISBLANKFunction;
	gFuncs[kISERRORFuncNr] = ISERRORFunction;
	gFuncs[kISNAFuncNr] = ISNAFunction;
	gFuncs[kISFORMULAFuncNr] = ISFORMULAFunction;
	gFuncs[kSUBSTITUTEFuncNr] = SUBSTITUTEFunction;
	gFuncs[kREPLACEFuncNr] = REPLACEFunction;
	gFuncs[kREPTFuncNr] = REPTFunction;
	gFuncs[kTEXTJOINFuncNr] = TEXTJOINFunction;
	gFuncs[kVALUEFuncNr] = VALUEFunction;
	gFuncs[kEXACTFuncNr] = EXACTFunction;
} /* InitFunctions */

void LoadPlugIns()
{
	char path[PATH_MAX], *name, plug[PATH_MAX];
	
	strcpy(path, gAppName.Path());
	name = strrchr(path, '/');
	if (name) *name = 0;

	strcat(path, "/Functions/");
	DIR *dir = opendir(path);

	if (!dir)
		return;
	
	struct dirent *dent;
	struct stat stbuf;

	int i, count = 0;
	image_id *plugs;
	
	plugs = (image_id *)MALLOC(sizeof(image_id));
	FailNil(plugs);
	
	while ((dent = readdir(dir)) != NULL)
	{
		strcpy(plug, path);
		strcat(plug, dent->d_name);
		status_t err = stat(plug, &stbuf);
		if (!err && S_ISREG(stbuf.st_mode) &&
			strcmp(dent->d_name, ".") && strcmp(dent->d_name, ".."))
		{
			image_id next;
			char *n;
			
			next = load_add_on(plug);
			if (next > B_ERROR &&
				get_image_symbol(next, "Name", B_SYMBOL_TYPE_DATA, (void**)&n) == B_NO_ERROR)
			{
				count++;
				plugs = (image_id *)REALLOC(plugs, count * sizeof(image_id));
				FailNil(plugs);
				plugs[count - 1] = next;
			}
		}
	}

	if (count == 0)
		return;
	
	gFuncArrayByNr = (FuncRec *)REALLOC(gFuncArrayByNr,
		sizeof(FuncRec) * (gFuncCount + count));
	FailNil(gFuncArrayByNr);

	gFuncDescriptions = (const char**)REALLOC(gFuncDescriptions,
		(gFuncCount + count) * sizeof(char*));
	FailNil(gFuncDescriptions);
	
	gFuncPasteStrings = (const char**)REALLOC(gFuncPasteStrings,
		(gFuncCount + count) * sizeof(char*));
	FailNil(gFuncPasteStrings);
	
	gFuncs = (Function*)REALLOC(gFuncs, (gFuncCount + count) * sizeof(Function));
	FailNil(gFuncs);

	for (i = 0; i < count; i++)
	{
		void (*setup)();
		char *name, *desc, *paste;
		long *argCnt;
		image_id plugid = plugs[i];
		
		if (get_image_symbol(plugid, "Setup", B_SYMBOL_TYPE_TEXT, (void**)&setup) == B_NO_ERROR && setup)
			(*setup)();
			
		FailOSErr(get_image_symbol(plugid, "Name", B_SYMBOL_TYPE_DATA,
			(void**)&name), errLoadingPlugins);
		if (strlen(name) > 9)
			THROW((errFunctionNameTooLong, name));
		strcpy(gFuncArrayByNr[gFuncCount].funcName, name);
	
		FailOSErr(get_image_symbol(plugid, "Description", B_SYMBOL_TYPE_DATA,
			(void**)&desc), errLoadingPlugins);
		gFuncDescriptions[gFuncCount] = desc;
	
		FailOSErr(get_image_symbol(plugid, "PasteString", B_SYMBOL_TYPE_DATA,
			(void**)&paste), errLoadingPlugins);
		gFuncPasteStrings[gFuncCount] = paste;
			
		FailOSErr(get_image_symbol(plugid, "Arguments", B_SYMBOL_TYPE_DATA,
			(void**)&argCnt), errLoadingPlugins);
		gFuncArrayByNr[gFuncCount].argCnt = *argCnt;
		gFuncArrayByNr[gFuncCount].funcNr = gFuncCount;
		gFuncArrayByNr[gFuncCount].groupNr = 8;
			
		FailOSErr(get_image_symbol(plugid, "Function", B_SYMBOL_TYPE_TEXT,
			(void**)&gFuncs[gFuncCount]), errLoadingPlugins);
			
		gFuncCount++;
	}
	
	FREE(plugs);
} /* LoadPlugIns */

bool GetDoubleArgument(Value *inStack,
	int inArgCnt, int inArgNr, double *outDouble)
{

	if (inArgNr <= inArgCnt && inStack[inArgNr - 1].fType == eNumData)
	{
		*outDouble = inStack[inArgNr - 1].fDouble;
		return true;
	}
	return false;
} /* GetDoubleArgument */

bool GetTextArgument(Value *inStack,
	int inArgCnt, int inArgNr, char *outText)
{

	if (inArgNr <= inArgCnt && inStack[inArgNr - 1].fType == eTextData)
	{
		strcpy(outText, inStack[inArgNr - 1].fText);
		return true;
	}
	return false;
} /* GetTextArgument */

bool GetBooleanArgument(Value *inStack, int inArgCnt,
	int inArgNr, bool *outbool)
{
	
	if (inArgNr <= inArgCnt && inStack[inArgNr - 1].fType == eBoolData)
	{
		*outbool = inStack[inArgNr - 1].fBool;
		return true;
	}
	return false;
} /* GetBooleanArgument */

bool GetTimeArgument(Value *inStack, int inArgCnt,
	int inArgNr, time_t *outTime)
{
	
	if (inArgNr <= inArgCnt && inStack[inArgNr - 1].fType == eTimeData)
	{
		*outTime = inStack[inArgNr - 1].fTime;
		return true;
	}
	return false;
} /* GetTimeArgument */

bool GetRangeArgument(Value *inStack, int inArgCnt,
	int inArgNr, range *outRange)
{

	if (inArgNr <= inArgCnt && inStack[inArgNr - 1].fType == eRangeData)
	{
		*outRange = inStack[inArgNr - 1].fRange;
		return true;
	}
	return false;
} /* GetRangeArgument */

CContainer* GetRangeContainer(Value *inStack, int inArgNr, CContainer *inFallback)
{
	CContainer *owner = inStack[inArgNr - 1].fRangeContainer;
	return owner ? owner : inFallback;
} /* GetRangeContainer */

void Return(Value *stack, ValueType type, void *data)
{
	switch (type)
	{
		case eNoData: stack[0].fType = eNoData; break;
		case eBoolData: stack[0] = *(bool *)data; break;
		case eNumData: stack[0] = *(double *)data; break;
		case eTimeData: stack[0] = *(time_t *)data; break;
		case eTextData: stack[0] = (char *)data; break;
		default: THROW((errIllegalResultType, type));
	}
} /* Return */

bool CheckForNanParameters(Value *inStack, int inArgCnt)
{
	while (--inArgCnt >= 0)
	{
		if (inStack[inArgCnt].fType == eNumData && isnan(inStack[inArgCnt].fDouble))
		{
			if (inArgCnt)
				inStack[0] = inStack[inArgCnt];
			return true;
		}
	}
	return false;
} // CheckForNanParameters
