/**********************************************************************

  Audacity: A Digital Audio Editor

  Export.h

  Dominic Mazzoni

**********************************************************************/

#ifndef __AUDACITY_EXPORT__
#define __AUDACITY_EXPORT__

#include <functional>
#include <vector>
#include <wx/filename.h> // member variable
#include "Identifier.h"
#include "FileNames.h" // for FileTypes

#include "Registry.h"
#include "ExportPlugin.h"

class AudacityProject;
class WaveTrack;
class ExportProgressListener;
namespace MixerOptions{ class Downmix; }
using MixerSpec = MixerOptions::Downmix;
using WaveTrackConstArray = std::vector < std::shared_ptr < const WaveTrack > >;

using ExportPluginArray = std::vector < std::unique_ptr< ExportPlugin > > ;

class  AUDACITY_DLL_API Exporter final
{
   struct ExporterItem;
public:
   
   enum class DownMixMode
   {
      None,
      Mono,
      Stereo,
      FormatDefined
   };

   using ExportPluginFactory =
      std::function< std::unique_ptr< ExportPlugin >() >;

   // Objects of this type are statically constructed in files implementing
   // subclasses of ExportPlugin
   // Register factories, not plugin objects themselves, which allows them
   // to have some fresh state variables each time export begins again
   // and to compute translated strings for the current locale
   struct AUDACITY_DLL_API RegisteredExportPlugin
      : public Registry::RegisteredItem<ExporterItem>
   {
      RegisteredExportPlugin(
         const Identifier &id, // an internal string naming the plug-in
         const ExportPluginFactory&,
         const Registry::Placement &placement = { wxEmptyString, {} } );
   };

   Exporter( AudacityProject &project );
   virtual ~Exporter();

   void Configure(const wxFileName& filename, int pluginIndex, int formatIndex);
   
   bool SetExportRange(double t0, double t1, bool selectedOnly, bool skipSilenceAtBeginning = false);
   
   MixerOptions::Downmix* CreateMixerSpec();
   
   DownMixMode SetUseStereoOrMonoOutput();
   
   bool CanMetaData() const;
   
   void Process(ExportProgressListener& progressListener);
   
   void Process(ExportProgressListener& progressListener,
                unsigned numChannels,
                const FileExtension &type, const wxString & filename,
                bool selectedOnly, double t0, double t1);

   const ExportPluginArray &GetPlugins();
   
   ExportPlugin* GetPlugin();
   ExportPlugin* GetPlugin(int pluginIndex);
   ExportPlugin* FindPluginByType(const FileExtension& type);
   
   int GetAutoExportFormat();
   int GetAutoExportSubFormat();
   wxFileName GetAutoExportFileName();

private:
   struct AUDACITY_DLL_API ExporterItem final : Registry::SingleItem {
      static Registry::GroupItemBase &Registry();
      ExporterItem(
         const Identifier &id, const Exporter::ExportPluginFactory &factory );
      Exporter::ExportPluginFactory mFactory;
   };

   void FixFilename();
   void ExportTracks(ExportProgressListener& progressListener);

private:
   AudacityProject *mProject;
   std::unique_ptr<MixerSpec> mMixerSpec;

   ExportPluginArray mPlugins;

   wxFileName mFilename;
   wxFileName mActualName;

   double mT0;
   double mT1;
   int mFormat{-1};
   int mSubFormat{-1};
   int mNumSelected{};
   bool mMono{};
   unsigned mNumMono;
   unsigned mChannels;
   bool mSelectedOnly;
};


AUDACITY_DLL_API TranslatableString AudacityExportCaptionStr();
AUDACITY_DLL_API TranslatableString AudacityExportMessageStr();

/// We have many Export errors that are essentially anonymous
/// and are distinguished only by an error code number.
/// Rather than repeat the code, we have it just once.
AUDACITY_DLL_API void ShowExportErrorDialog(wxString ErrorCode,
   TranslatableString message = AudacityExportMessageStr(),
   const TranslatableString& caption = AudacityExportCaptionStr(),
   bool allowReporting = true);

AUDACITY_DLL_API
void ShowDiskFullExportErrorDialog(const wxFileNameWrapper &fileName);

#endif
