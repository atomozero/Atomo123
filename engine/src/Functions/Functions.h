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
	Functions.h
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

enum {
	kABSFuncNr,
	kACOSFuncNr,
	kANNUITYFuncNr,
	kASCFuncNr,
	kASINFuncNr,
	kATANFuncNr,
	kAVGFuncNr,
	kCEILINGFuncNr,
	kCELLFuncNr,
	kCHOOSEFuncNr,
	kCHRFuncNr,
	kCOLUMNFuncNr,
	kCOMPOUNDFuncNr,
	kCOSFuncNr,
	kCOTFuncNr,
	kCOUNTFuncNr,
	kDATEFuncNr,
	kDAYFuncNr,
	kDBFuncNr,
	kDOCUMENTFuncNr,
	kDOWFuncNr,
	kERRFuncNr,
	kERRORFuncNr,
	kEXPFuncNr,
	kFALSEFuncNr,
	kFLOORFuncNr,
	kFRACFuncNr,
	kFVFuncNr,
	kHINDEXFuncNr,
	kHLOOKUPFuncNr,
	kHOURFuncNr,
	kIFFuncNr,
	kIFERRFuncNr,
	kINTFuncNr,
	kIRRFuncNr,
	kISNULLFuncNr,
	kISNUMFuncNr,
	kISTEXTFuncNr,
	kLEFTFuncNr,
	kLENGTHFuncNr,
	kLNFuncNr,
	kLOGFuncNr,
	kMAXFuncNr,
	kMIDFuncNr,
	kMINFuncNr,
	kMINUTEFuncNr,
	kMODFuncNr,
	kMONTHFuncNr,
	kNAFuncNr,
	kNCOLSFuncNr,
	kNOWFuncNr,
	kNPVFuncNr,
	kNROWSFuncNr,
	kNUM2CFuncNr,
	kNUMPAGESFuncNr,
	kOFFSETFuncNr,
	kPAGEFuncNr,
	kPIFuncNr,
	kPMTFuncNr,
	kPVFuncNr,
	kRANDOMFuncNr,
	kRIGHTFuncNr,
	kROUNDFuncNr,
	kROWFuncNr,
	kSECONDFuncNr,
	kSIGNFuncNr,
	kSINFuncNr,
	kSLFuncNr,
	kSOYDFuncNr,
	kSQRTFuncNr,
	kSTDDEVFuncNr,
	kSUMFuncNr,
	kTANFuncNr,
	kTIMEFuncNr,
	kTIME2CFuncNr,
	kVARIANCEFuncNr,
	kVINDEXFuncNr,
	kVLOOKUPFuncNr,
	kYEARFuncNr,
	kTRUEFuncNr,
	kORFuncNr,
	kANDFuncNr,
	kPOWERFuncNr,
	kDEC2HEXFuncNr,
	kATAN2FuncNr,
	kCOUNTAFuncNr,
	kSUMIFFuncNr,
	kCOUNTIFFuncNr,
	kAVERAGEIFFuncNr,
	kTRIMFuncNr,
	kUPPERFuncNr,
	kLOWERFuncNr,
	kPROPERFuncNr,
	kFINDFuncNr,
	kSEARCHFuncNr,
	kCONCATFuncNr,
	kMEDIANFuncNr,
	kMODEFuncNr,
	kIFERRORFuncNr,
	kINDEXFuncNr,
	kMATCHFuncNr,
	// XLOOKUP/IFS (Fase 14): funzioni Excel piu' recenti del formato
	// dichiarato del file, scritte da Excel col prefisso "_xlfn."
	// davanti (vedi GetFunctionNr in Utils.cpp) -- bug reale segnalato
	// dall'utente, un file XLSX reale con XLOOKUP mostrava il testo
	// letterale della formula invece del valore calcolato.
	kXLOOKUPFuncNr,
	kIFSFuncNr,
	// COUNTIFS (Fase 14): a differenza di COUNTIF sopra (un solo
	// intervallo/criterio), conta un AND fra piu' coppie intervallo/
	// criterio -- vedi COUNTIFSFunction in Functions.math.cpp.
	kCOUNTIFSFuncNr,
	// ROUNDUP/ROUNDDOWN/TEXT (Fase 14): assenti dalle funzioni
	// originali, scoperte mancanti analizzando altri file XLSX reali
	// dell'utente (analisi_funzioni_xls.md). CONCATENATE (11
	// caratteri, non entra nel campo a lunghezza fissa della risorsa
	// 'Func', stesso limite di CEILING.MATH) e' un alias diretto di
	// CONCAT, gia' esistente -- non serve una voce propria qui.
	kROUNDUPFuncNr,
	kROUNDDOWNFuncNr,
	kTEXTFuncNr,
	// NOT/XOR/SWITCH/IFNA/ISBLANK/ISERROR/ISNA/ISFORMULA (Fase 26, vedi
	// ROADMAP.md "v3.0 Consolidation"): assenti dalle funzioni
	// originali di Sum-It, mancanti confrontando la tabella con
	// l'elenco standard di Excel.
	kNOTFuncNr,
	kXORFuncNr,
	kSWITCHFuncNr,
	kIFNAFuncNr,
	kISBLANKFuncNr,
	kISERRORFuncNr,
	kISNAFuncNr,
	kISFORMULAFuncNr,
	// SUBSTITUTE/REPLACE/REPT/TEXTJOIN/VALUE/EXACT (Fase 26, vedi
	// ROADMAP.md "v3.0 Consolidation"): assenti dalle funzioni
	// originali di Sum-It, mancanti confrontando la tabella con
	// l'elenco standard di Excel.
	kSUBSTITUTEFuncNr,
	kREPLACEFuncNr,
	kREPTFuncNr,
	kTEXTJOINFuncNr,
	kVALUEFuncNr,
	kEXACTFuncNr,
	// TODAY/NETWORKDAYS/WORKDAY/EDATE/EOMONTH/DATEDIF (Fase 26, vedi
	// ROADMAP.md "v3.0 Consolidation"): assenti dalle funzioni
	// originali di Sum-It, mancanti confrontando la tabella con
	// l'elenco standard di Excel.
	kTODAYFuncNr,
	kNETWORKDAYSFuncNr,
	kWORKDAYFuncNr,
	kEDATEFuncNr,
	kEOMONTHFuncNr,
	kDATEDIFFuncNr,
	// SUMPRODUCT/AVERAGEIFS/MAXIFS/MINIFS/RANK/LARGE/SMALL/SUBTOTAL
	// (Fase 26, vedi ROADMAP.md "v3.0 Consolidation"): assenti dalle
	// funzioni originali di Sum-It, mancanti confrontando la tabella
	// con l'elenco standard di Excel.
	kSUMPRODUCTFuncNr,
	kAVERAGEIFSFuncNr,
	kMAXIFSFuncNr,
	kMINIFSFuncNr,
	kRANKFuncNr,
	kLARGEFuncNr,
	kSMALLFuncNr,
	kSUBTOTALFuncNr,
	// INDIRECT/ADDRESS/XMATCH (Fase 26, vedi ROADMAP.md "v3.0
	// Consolidation"): assenti dalle funzioni originali di Sum-It,
	// mancanti confrontando la tabella con l'elenco standard di Excel.
	kINDIRECTFuncNr,
	kADDRESSFuncNr,
	kXMATCHFuncNr,
	// SEQUENCE (Fase 29, vedi ROADMAP.md "v3.0 Consolidation", ultimo
	// elemento del backlog): l'unica funzione "spill" di questa prima
	// versione -- vedi il commento su CContainer::ApplySpill in
	// Container.h per il design completo. Usata annidata/non come
	// intera formula di una cella si comporta come uno scalare
	// (restituisce solo il primo elemento, "start"), niente spill in
	// quel caso: limite noto, non un errore.
	kSEQUENCEFuncNr,
	// RATE (Fase 30, vedi ROADMAP.md "Path to full Excel parity" Tier
	// 1): l'unica delle sei funzioni finanziarie elencate nel roadmap
	// che mancava davvero -- NPV/IRR/PMT/FV/PV erano gia' tutte
	// implementate da prima (verificato leggendo Functions.finance.cpp
	// prima di aggiungere altro).
	kRATEFuncNr,
	// UNIQUE (Fase 34, "Path to full Excel parity" Tier 2, "Dynamic
	// arrays beyond SEQUENCE"): seconda funzione "spill" di Atomo123,
	// stesso meccanismo di SEQUENCE sopra (CContainer::ApplySpill).
	kUNIQUEFuncNr,
	kFunctionCount
};

void ABSFunction(Value *stack, int argCnt, CContainer *cells);
void ACOSFunction(Value *stack, int argCnt, CContainer *cells);
void ANNUITYFunction(Value *stack, int argCnt, CContainer *cells);
void ASCFunction(Value *stack, int argCnt, CContainer *cells);
void ASINFunction(Value *stack, int argCnt, CContainer *cells);
void ATANFunction(Value *stack, int argCnt, CContainer *cells);
void AVGFunction(Value *stack, int argCnt, CContainer *cells);
void CEILINGFunction(Value *stack, int argCnt, CContainer *cells);
void CELLFunction(Value *stack, int argCnt, CContainer *cells);
void CHOOSEFunction(Value *stack, int argCnt, CContainer *cells);
void CHRFunction(Value *stack, int argCnt, CContainer *cells);
void COLUMNFunction(Value *stack, int argCnt, CContainer *cells);
void COMPOUNDFunction(Value *stack, int argCnt, CContainer *cells);
void COSFunction(Value *stack, int argCnt, CContainer *cells);
void COTFunction(Value *stack, int argCnt, CContainer *cells);
void COUNTFunction(Value *stack, int argCnt, CContainer *cells);
void DATEFunction(Value *stack, int argCnt, CContainer *cells);
void DAYFunction(Value *stack, int argCnt, CContainer *cells);
void DBFunction(Value *stack, int argCnt, CContainer *cells);
void DOCUMENTFunction(Value *stack, int argCnt, CContainer *cells);
void DOWFunction(Value *stack, int argCnt, CContainer *cells);
void ERRFunction(Value *stack, int argCnt, CContainer *cells);
void ERRORFunction(Value *stack, int argCnt, CContainer *cells);
void EXPFunction(Value *stack, int argCnt, CContainer *cells);
void FALSEFunction(Value *stack, int argCnt, CContainer *cells);
void FLOORFunction(Value *stack, int argCnt, CContainer *cells);
void FRACFunction(Value *stack, int argCnt, CContainer *cells);
void FVFunction(Value *stack, int argCnt, CContainer *cells);
void HINDEXFunction(Value *stack, int argCnt, CContainer *cells);
void HLOOKUPFunction(Value *stack, int argCnt, CContainer *cells);
void HOURFunction(Value *stack, int argCnt, CContainer *cells);
void IFFunction(Value *stack, int argCnt, CContainer *cells);
void IFERRFunction(Value *stack, int argCnt, CContainer *cells);
void INTFunction(Value *stack, int argCnt, CContainer *cells);
void IRRFunction(Value *stack, int argCnt, CContainer *cells);
void ISNULLFunction(Value *stack, int argCnt, CContainer *cells);
void ISNUMFunction(Value *stack, int argCnt, CContainer *cells);
void ISTEXTFunction(Value *stack, int argCnt, CContainer *cells);
void LEFTFunction(Value *stack, int argCnt, CContainer *cells);
void LENGTHFunction(Value *stack, int argCnt, CContainer *cells);
void LNFunction(Value *stack, int argCnt, CContainer *cells);
void LOGFunction(Value *stack, int argCnt, CContainer *cells);
void MAXFunction(Value *stack, int argCnt, CContainer *cells);
void MIDFunction(Value *stack, int argCnt, CContainer *cells);
void MINFunction(Value *stack, int argCnt, CContainer *cells);
void MINUTEFunction(Value *stack, int argCnt, CContainer *cells);
void MODFunction(Value *stack, int argCnt, CContainer *cells);
void MONTHFunction(Value *stack, int argCnt, CContainer *cells);
void NAFunction(Value *stack, int argCnt, CContainer *cells);
void NCOLSFunction(Value *stack, int argCnt, CContainer *cells);
void NOWFunction(Value *stack, int argCnt, CContainer *cells);
void NPVFunction(Value *stack, int argCnt, CContainer *cells);
void NROWSFunction(Value *stack, int argCnt, CContainer *cells);
void NUM2CFunction(Value *stack, int argCnt, CContainer *cells);
void NUMPAGESFunction(Value *stack, int argCnt, CContainer *cells);
void OFFSETFunction(Value *stack, int argCnt, CContainer *cells);
void PAGEFunction(Value *stack, int argCnt, CContainer *cells);
void PIFunction(Value *stack, int argCnt, CContainer *cells);
void PMTFunction(Value *stack, int argCnt, CContainer *cells);
void PVFunction(Value *stack, int argCnt, CContainer *cells);
void RANDOMFunction(Value *stack, int argCnt, CContainer *cells);
void RATEFunction(Value *stack, int argCnt, CContainer *cells);
void RIGHTFunction(Value *stack, int argCnt, CContainer *cells);
void ROUNDFunction(Value *stack, int argCnt, CContainer *cells);
void ROWFunction(Value *stack, int argCnt, CContainer *cells);
void SECONDFunction(Value *stack, int argCnt, CContainer *cells);
void SIGNFunction(Value *stack, int argCnt, CContainer *cells);
void SINFunction(Value *stack, int argCnt, CContainer *cells);
void SLFunction(Value *stack, int argCnt, CContainer *cells);
void SOYDFunction(Value *stack, int argCnt, CContainer *cells);
void SQRTFunction(Value *stack, int argCnt, CContainer *cells);
void STDDEVFunction(Value *stack, int argCnt, CContainer *cells);
void SUMFunction(Value *stack, int argCnt, CContainer *cells);
void TANFunction(Value *stack, int argCnt, CContainer *cells);
void TIMEFunction(Value *stack, int argCnt, CContainer *cells);
void TIME2CFunction(Value *stack, int argCnt, CContainer *cells);
void VARIANCEFunction(Value *stack, int argCnt, CContainer *cells);
void VINDEXFunction(Value *stack, int argCnt, CContainer *cells);
void VLOOKUPFunction(Value *stack, int argCnt, CContainer *cells);
void YEARFunction(Value *stack, int argCnt, CContainer *cells);
void TRUEFunction(Value *stack, int argCnt, CContainer *cells);
void ANDFunction(Value *stack, int argCnt, CContainer *cells);
void ORFunction(Value *stack, int argCnt, CContainer *cells);
void POWERFunction(Value *stack, int argCnt, CContainer *cells);
void DEC2HEXFunction(Value *stack, int argCnt, CContainer *cells);
void ATAN2Function(Value *stack, int argCnt, CContainer *cells);
void COUNTAFunction(Value *stack, int argCnt, CContainer *cells);
void SUMIFFunction(Value *stack, int argCnt, CContainer *cells);
void COUNTIFFunction(Value *stack, int argCnt, CContainer *cells);
void COUNTIFSFunction(Value *stack, int argCnt, CContainer *cells);
void ROUNDUPFunction(Value *stack, int argCnt, CContainer *cells);
void ROUNDDOWNFunction(Value *stack, int argCnt, CContainer *cells);
void TEXTFunction(Value *stack, int argCnt, CContainer *cells);
void AVERAGEIFFunction(Value *stack, int argCnt, CContainer *cells);
void TRIMFunction(Value *stack, int argCnt, CContainer *cells);
void UPPERFunction(Value *stack, int argCnt, CContainer *cells);
void LOWERFunction(Value *stack, int argCnt, CContainer *cells);
void PROPERFunction(Value *stack, int argCnt, CContainer *cells);
void FINDFunction(Value *stack, int argCnt, CContainer *cells);
void SEARCHFunction(Value *stack, int argCnt, CContainer *cells);
void CONCATFunction(Value *stack, int argCnt, CContainer *cells);
void MEDIANFunction(Value *stack, int argCnt, CContainer *cells);
void MODEFunction(Value *stack, int argCnt, CContainer *cells);
void INDEXFunction(Value *stack, int argCnt, CContainer *cells);
void MATCHFunction(Value *stack, int argCnt, CContainer *cells);
void XLOOKUPFunction(Value *stack, int argCnt, CContainer *cells);
void IFSFunction(Value *stack, int argCnt, CContainer *cells);
void NOTFunction(Value *stack, int argCnt, CContainer *cells);
void XORFunction(Value *stack, int argCnt, CContainer *cells);
void SWITCHFunction(Value *stack, int argCnt, CContainer *cells);
void IFNAFunction(Value *stack, int argCnt, CContainer *cells);
void ISBLANKFunction(Value *stack, int argCnt, CContainer *cells);
void ISERRORFunction(Value *stack, int argCnt, CContainer *cells);
void ISNAFunction(Value *stack, int argCnt, CContainer *cells);
void ISFORMULAFunction(Value *stack, int argCnt, CContainer *cells);
void SUBSTITUTEFunction(Value *stack, int argCnt, CContainer *cells);
void REPLACEFunction(Value *stack, int argCnt, CContainer *cells);
void REPTFunction(Value *stack, int argCnt, CContainer *cells);
void TEXTJOINFunction(Value *stack, int argCnt, CContainer *cells);
void VALUEFunction(Value *stack, int argCnt, CContainer *cells);
void EXACTFunction(Value *stack, int argCnt, CContainer *cells);
void TODAYFunction(Value *stack, int argCnt, CContainer *cells);
void NETWORKDAYSFunction(Value *stack, int argCnt, CContainer *cells);
void WORKDAYFunction(Value *stack, int argCnt, CContainer *cells);
void EDATEFunction(Value *stack, int argCnt, CContainer *cells);
void EOMONTHFunction(Value *stack, int argCnt, CContainer *cells);
void DATEDIFFunction(Value *stack, int argCnt, CContainer *cells);
void SUMPRODUCTFunction(Value *stack, int argCnt, CContainer *cells);
void AVERAGEIFSFunction(Value *stack, int argCnt, CContainer *cells);
void MAXIFSFunction(Value *stack, int argCnt, CContainer *cells);
void MINIFSFunction(Value *stack, int argCnt, CContainer *cells);
void RANKFunction(Value *stack, int argCnt, CContainer *cells);
void LARGEFunction(Value *stack, int argCnt, CContainer *cells);
void SMALLFunction(Value *stack, int argCnt, CContainer *cells);
void SUBTOTALFunction(Value *stack, int argCnt, CContainer *cells);
void INDIRECTFunction(Value *stack, int argCnt, CContainer *cells);
void ADDRESSFunction(Value *stack, int argCnt, CContainer *cells);
void XMATCHFunction(Value *stack, int argCnt, CContainer *cells);
void SEQUENCEFunction(Value *stack, int argCnt, CContainer *cells);
void UNIQUEFunction(Value *stack, int argCnt, CContainer *cells);

#endif
