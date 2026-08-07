// Replay script for SBS-GEn
// October 2022
//
// GEp-style single-file replay version:
//   - keeps detector/apparatus setup unchanged
//   - replays ONE selected stream/segment at a time using THaRun
//
// Example:
//   replay_gen_stream_gepstyle(10491, -1, 1, "e1209016", 3, 1, 0, 0, 1, 0, 1)
//   -> replays stream 0, segment 3 only

#include <iostream>
#include <vector>
#include <string>

#include "TSystem.h"
#include "THaGlobals.h"
#include "TString.h"
#include "TFile.h"
#include "TList.h"
#include "TObject.h"
#include "TClonesArray.h"
#include "TDatime.h"

#include "THaRun.h"
#include "THaEvData.h"
#include "THaAnalyzer.h"
#include "THaVarList.h"
#include "THaInterface.h"
#include "THaGoldenTrack.h"
#include "THaPrimaryKine.h"
#include "THaDecData.h"

#include "SBSBigBite.h"
#include "SBSBBShower.h"
#include "SBSBBTotalShower.h"
#include "SBSGRINCH.h"
#include "SBSEArm.h"
#include "SBSHCal.h"
#include "SBSTimingHodoscope.h"
#include "SBSGEMSpectrometerTracker.h"
#include "SBSGEMTrackerBase.h"
#include "SBSRasteredBeam.h"
#include "LHRSScalerEvtHandler.h"
#include "SBSScalerEvtHandler.h"
#include "SBSScalerHelicity.h"

using namespace std;

void replay_gen_stream(
    UInt_t runnum=10491,
    Long_t nevents=-1,
    Long_t firstevent=1,
    const char *fname_prefix="e1209016",
    UInt_t firstsegment=0,
    UInt_t maxsegments=1,   // kept for interface compatibility; not used except warning
    Int_t maxstream=2,      // kept for interface compatibility; not used except warning
    Int_t pedestalmode=0,
    Int_t cmplots=1,
    Int_t firststream=0,
    Int_t usesbsgems=1)
{
  if( maxsegments != 1 ) {
    cout << "Warning: this GEp-style version processes one segment at a time.\n"
         << "Using firstsegment = " << firstsegment << " only." << endl;
  }

  if( maxstream != firststream ) {
    cout << "Warning: this GEp-style version processes one stream at a time.\n"
         << "Using firststream = " << firststream << " only." << endl;
  }

  THaAnalyzer* analyzer = new THaAnalyzer;

  // ---------------------------------------------------------------------------
  // Detector/apparatus setup (kept essentially unchanged)
  // ---------------------------------------------------------------------------

  SBSBigBite* bigbite = new SBSBigBite("bb", "BigBite spectrometer" );

  SBSBBTotalShower* ts = new SBSBBTotalShower("ts", "sh", "ps", "BigBite shower");
  ts->SetDataOutputLevel(0);
  bigbite->AddDetector(ts);
  ts->SetStoreEmptyElements(kFALSE);
  ts->GetShower()->SetStoreEmptyElements(kFALSE);
  ts->GetPreShower()->SetStoreEmptyElements(kFALSE);

  SBSGenericDetector* bbtrig = new SBSGenericDetector("bbtrig","BigBite shower ADC trig");
  bbtrig->SetModeADC(SBSModeADC::kADC);
  bbtrig->SetModeTDC(SBSModeTDC::kTDC);
  bbtrig->SetStoreEmptyElements(kFALSE);
  bigbite->AddDetector(bbtrig);

  SBSGenericDetector* tdctrig = new SBSGenericDetector("tdctrig","BigBite shower TDC trig");
  tdctrig->SetModeADC(SBSModeADC::kNone);
  tdctrig->SetModeTDC(SBSModeTDC::kTDCSimple);
  tdctrig->SetStoreEmptyElements(kFALSE);
  bigbite->AddDetector(tdctrig);

  SBSGRINCH *grinch_tdc = new SBSGRINCH("grinch_tdc","GRINCH TDC data");
  SBSGenericDetector *grinch_adc = new SBSGenericDetector("grinch_adc","GRINCH ADC data");
  grinch_adc->SetModeADC(SBSModeADC::kWaveform);
  grinch_adc->SetModeTDC(SBSModeTDC::kNone);
  grinch_adc->SetStoreEmptyElements(kFALSE);
  grinch_adc->SetStoreRawHits(kFALSE);

  grinch_tdc->SetModeTDC(SBSModeTDC::kTDC);
  grinch_tdc->SetModeADC(SBSModeADC::kNone);
  grinch_tdc->SetStoreEmptyElements(kFALSE);
  grinch_tdc->SetStoreRawHits(kFALSE);
  grinch_tdc->SetDisableRefTDC(true);
  bigbite->AddDetector(grinch_tdc);

  SBSTimingHodoscope* hodotdc = new SBSTimingHodoscope("hodotdc", "BigBite hodo");
  hodotdc->SetModeTDC(SBSModeTDC::kTDC);
  hodotdc->SetModeADC(SBSModeADC::kNone);
  hodotdc->SetStoreEmptyElements(kFALSE);
  hodotdc->SetDataOutputLevel(1);

  SBSTimingHodoscope* hodoadc = new SBSTimingHodoscope("hodoadc", "BigBite hodo");
  hodoadc->SetModeTDC(SBSModeTDC::kNone);
  hodoadc->SetModeADC(SBSModeADC::kADC);
  hodoadc->SetStoreEmptyElements(kFALSE);
  hodoadc->SetStoreRawHits(kFALSE);

  // Preserving your original behavior, even though it looks a bit odd:
  hodotdc->SetDataOutputLevel(0);

  bigbite->AddDetector(hodotdc);
  bigbite->AddDetector(hodoadc);

  SBSGEMSpectrometerTracker *bbgem =
      new SBSGEMSpectrometerTracker("gem", "BigBite Hall A GEM data");

  bool pm = (pedestalmode != 0);
  bbgem->SetPedestalMode(pm);
  bbgem->SetMakeCommonModePlots(cmplots);
  bigbite->AddDetector(bbgem);

  gHaApps->Add(bigbite);

  SBSEArm *harm = new SBSEArm("sbs","Hadron Arm with HCal");

  SBSHCal* hcal = new SBSHCal("hcal","HCAL");
  hcal->SetStoreEmptyElements(kFALSE);
  harm->AddDetector(hcal);

  SBSGenericDetector* sbstrig = new SBSGenericDetector("trig","HCal trigs");
  sbstrig->SetModeADC(SBSModeADC::kWaveform);
  sbstrig->SetStoreEmptyElements(kFALSE);
  harm->AddDetector(sbstrig);

  SBSGEMSpectrometerTracker *sbsgem =
      new SBSGEMSpectrometerTracker("gem", "Super BigBite Hall A GEM data");
  sbsgem->SetPedestalMode(pm);
  sbsgem->SetMakeCommonModePlots(cmplots);
  if( usesbsgems != 0 ) harm->AddDetector(sbsgem);

  SBSGenericDetector *tdctrig_sbs = new SBSGenericDetector("tdctrig", "SBS trigger TDCs");
  tdctrig_sbs->SetModeADC(SBSModeADC::kNone);
  tdctrig_sbs->SetModeTDC(SBSModeTDC::kTDCSimple);
  tdctrig_sbs->SetStoreEmptyElements(kFALSE);
  harm->AddDetector(tdctrig_sbs);

  gHaApps->Add(harm);

  THaApparatus* decL = new THaDecData("DL","Misc. Decoder Data");
  gHaApps->Add(decL);

  THaApparatus* Lrb = new SBSRasteredBeam("Lrb","Raster Beamline for FADC");
  gHaApps->Add(Lrb);
  Lrb->AddDetector(new SBSScalerHelicity("scalhel","Scaler Helicity info"));

  THaApparatus* sbs = new SBSRasteredBeam("SBSrb","Raster Beamline for FADC");
  gHaApps->Add(sbs);

  gHaPhysics->Add(new THaGoldenTrack("BB.gold", "BigBite golden track", "bb"));
  gHaPhysics->Add(new THaPrimaryKine("e.kine", "electron kinematics", "bb", 0.0, 0.938272));

  LHRSScalerEvtHandler *lScaler =
      new LHRSScalerEvtHandler("Left","HA scaler event type 140");
  gHaEvtHandlers->Add(lScaler);

  SBSScalerEvtHandler *sbsScaler =
      new SBSScalerEvtHandler("sbs","SBS Scaler Bank event type 1");
  sbsScaler->SetUseFirstEvent(kTRUE);
  gHaEvtHandlers->Add(sbsScaler);

  // ---------------------------------------------------------------------------
  // Path setup
  // ---------------------------------------------------------------------------

  TString prefix = gSystem->Getenv("DATA_DIR");

  vector<TString> pathlist;
  if( !prefix.IsNull() )
    pathlist.push_back( prefix );

  if( prefix != "/adaqeb[1-3]/data1" )
    pathlist.push_back( "/adaqeb[1-3]/data1" );

  if( prefix != "/adaq1/data1/sbs" )
    pathlist.push_back( "/adaq1/data1/sbs" );

  if( prefix != "/cache/mss/halla/sbs/raw" )
    pathlist.push_back( "/cache/mss/halla/sbs/raw" );

  if( prefix != "/cache/mss/halla/sbs/GEnII/raw" )
    pathlist.push_back( "/cache/mss/halla/sbs/GEnII/raw" );

  for( const auto& path: pathlist ) {
    cout << "search paths = " << path.Data() << endl;
  }

  // ---------------------------------------------------------------------------
  // Build exactly one filename
  // ---------------------------------------------------------------------------

  TString ftest(fname_prefix);
  bool test_data = ( ftest == "bbgem" || ftest == "e1209019_trigtest" );

  Int_t istream = firststream;
  UInt_t iseg = firstsegment;

  TString codafilename;
  if( test_data )
    codafilename.Form("%s_%u.evio.%u", fname_prefix, runnum, istream);
  else
    codafilename.Form("%s_%u.evio.%d.%u", fname_prefix, runnum, istream, iseg);

  cout << "codafilename = " << codafilename << endl;

  // Explicit existence check, GEp-style
  bool fileexists = false;
  TString foundpath = "";

  for( const auto& path : pathlist ) {
    TString searchname;
    searchname.Form("%s/%s", path.Data(), codafilename.Data());
    cout << "Checking: " << searchname << endl;

    if( !gSystem->AccessPathName(searchname.Data()) ) {
      fileexists = true;
      foundpath = searchname;
      break;
    }
  }

  if( !fileexists ) {
    cerr << "No file found for " << codafilename << endl;
    return;
  }

  cout << "Found file: " << foundpath << endl;

  // ---------------------------------------------------------------------------
  // Create single THaRun (GEp-style)
  // ---------------------------------------------------------------------------

  THaRun* run = new THaRun(pathlist, codafilename.Data(), "GEn run");

  analyzer->SetCountMode(THaAnalyzer::kCountPhysics);

  run->SetFirstEvent(firstevent);
  if( nevents > 0 )
    run->SetLastEvent(firstevent + nevents - 1);

  run->SetDataRequired(THaRunBase::kDate | THaRunBase::kRunNumber);

  Int_t st = run->Init();
  if( st != THaRunBase::READ_OK ) {
    cerr << "========= Error initializing run" << endl;
    return;
  }

  cout << "Initialized run successfully." << endl;
  cout << "Run number   = " << runnum << endl;
  cout << "Stream       = " << istream << endl;
  cout << "Segment      = " << iseg << endl;

  // ---------------------------------------------------------------------------
  // Output / analyzer config
  // ---------------------------------------------------------------------------

  prefix = gSystem->Getenv("OUT_DIR");

  TString outfilename;
  if( nevents > 0 ) {
    outfilename.Form("%s/%s_replayed_%u_stream%d_seg%u_firstevent%ld_nevent%ld.root",
                     prefix.Data(), fname_prefix, runnum,
                     istream, iseg, firstevent, nevents);
  } else {
    outfilename.Form("%s/%s_fullreplay_%u_stream%d_seg%u.root",
                     prefix.Data(), fname_prefix, runnum,
                     istream, iseg);
  }

  analyzer->EnableHelicity();
  analyzer->SetVerbosity(2);
  analyzer->SetMarkInterval(100);
  analyzer->EnableBenchmarks();

  analyzer->SetOutFile(outfilename.Data());

  prefix = gSystem->Getenv("LOG_DIR");
  analyzer->SetSummaryFile(Form("%s/%s_%u_stream%d_seg%u.log",
                                prefix.Data(), fname_prefix, runnum,
                                istream, iseg));

  prefix = gSystem->Getenv("SBS_REPLAY");
  prefix += "/replay/";

  TString odef_filename = "replay_gen.odef";
  if( usesbsgems == 0 )
    odef_filename = "replay_gen_noSBSGEMs.odef";
  odef_filename.Prepend(prefix);

  analyzer->SetOdefFile(odef_filename);

  TString cdef_filename = "replay_gen.cdef";
  cdef_filename.Prepend(prefix);
  analyzer->SetCutFile(cdef_filename);

  st = analyzer->Init(run);
  if( st != 0 ) {
    cerr << "========= Error initializing analyzer" << endl;
    return;
  }

  analyzer->Process(run);
}
