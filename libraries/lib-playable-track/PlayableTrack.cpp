/*!********************************************************************

  Audacity: A Digital Audio Editor

  PlayableTrack.cpp

  Dominic Mazzoni

  Paul Licameli split from Track.cpp

*******************************************************************//**

\class AudioTrack
\brief A Track that can load/save audio data to/from XML.

\class PlayableTrack
\brief An AudioTrack that can be played and stopped.

*//*******************************************************************/
#include "PlayableTrack.h"

namespace {
struct MuteAndSolo : ClientData::Cloneable<> {
   MuteAndSolo() = default;
   MuteAndSolo(const MuteAndSolo &);
   MuteAndSolo& operator=(const MuteAndSolo &) = delete;
   ~MuteAndSolo() override;
   std::unique_ptr<ClientData::Cloneable<>> Clone() const override;

   static MuteAndSolo &Get(PlayableTrack &track);
   static const MuteAndSolo &Get(const PlayableTrack &track);

   bool GetMute() const;
   void SetMute(bool value);
   bool GetSolo() const;
   void SetSolo(bool value);

private:
   //! Atomic because it may be read by worker threads in playback
   std::atomic<bool> mMute{ false };
   //! Atomic because it may be read by worker threads in playback
   std::atomic<bool> mSolo{ false };
};

static const ChannelGroup::Attachments::RegisteredFactory
muteAndSoloFactory{ [](auto &) { return std::make_unique<MuteAndSolo>(); } };

//! Copy can't be generated by default because of mutable members
MuteAndSolo::MuteAndSolo(const MuteAndSolo &other) {
   SetMute(other.GetMute());
   SetSolo(other.GetSolo());
}

MuteAndSolo::~MuteAndSolo() = default;

std::unique_ptr<ClientData::Cloneable<>> MuteAndSolo::Clone() const {
   return std::make_unique<MuteAndSolo>(*this);
}

MuteAndSolo &MuteAndSolo::Get(PlayableTrack &track) {
   return track.GetGroupData().Attachments
      ::Get<MuteAndSolo>(muteAndSoloFactory);
}

const MuteAndSolo &MuteAndSolo::Get(const PlayableTrack &track)
{
   return Get(const_cast<PlayableTrack &>(track));
}

bool MuteAndSolo::GetMute() const
{
   return mMute.load(std::memory_order_relaxed);
}

void MuteAndSolo::SetMute(bool value)
{
   mMute.store(value, std::memory_order_relaxed);
}

bool MuteAndSolo::GetSolo() const
{
   return mSolo.load(std::memory_order_relaxed);
}

void MuteAndSolo::SetSolo(bool value)
{
   mSolo.store(value, std::memory_order_relaxed);
}
}


AudioTrack::AudioTrack() : Track{}
{
}

AudioTrack::AudioTrack(const Track &orig, ProtectedCreationArg &&a)
   : Track{ orig, std::move(a) }
{
}

PlayableTrack::PlayableTrack() : AudioTrack{}
{
}

PlayableTrack::PlayableTrack(
   const PlayableTrack &orig, ProtectedCreationArg &&a
)  : AudioTrack{ orig, std::move(a) }
{
}

void PlayableTrack::SetMute(bool m)
{
   if (DoGetMute() != m) {
      DoSetMute(m);
      Notify(true);
   }
}

void PlayableTrack::SetSolo(bool s)
{
   if (DoGetSolo() != s) {
      DoSetSolo(s);
      Notify(true);
   }
}

bool PlayableTrack::DoGetMute() const
{
   return MuteAndSolo::Get(*this).GetMute();
}

void PlayableTrack::DoSetMute(bool value)
{
   MuteAndSolo::Get(*this).SetMute(value);
}

bool PlayableTrack::DoGetSolo() const
{
   return MuteAndSolo::Get(*this).GetSolo();
}

void PlayableTrack::DoSetSolo(bool value)
{
   MuteAndSolo::Get(*this).SetSolo(value);
}

// Serialize, not with tags of its own, but as attributes within a tag.
void PlayableTrack::WriteXMLAttributes(XMLWriter &xmlFile) const
{
   xmlFile.WriteAttr(wxT("mute"), DoGetMute());
   xmlFile.WriteAttr(wxT("solo"), DoGetSolo());
   AudioTrack::WriteXMLAttributes(xmlFile);
}

// Return true iff the attribute is recognized.
bool PlayableTrack::HandleXMLAttribute(const std::string_view &attr, const XMLAttributeValueView &value)
{
   long nValue;

   if (attr == "mute" && value.TryGet(nValue)) {
      DoSetMute(nValue != 0);
      return true;
   }
   else if (attr == "solo" && value.TryGet(nValue)) {
      DoSetSolo(nValue != 0);
      return true;
   }

   return AudioTrack::HandleXMLAttribute(attr, value);
}

auto AudioTrack::ClassTypeInfo() -> const TypeInfo &
{
   static Track::TypeInfo info{
      { "audio", "audio", XO("Audio Track") },
      false, &Track::ClassTypeInfo() };
   return info;
}

auto PlayableTrack::ClassTypeInfo() -> const TypeInfo &
{
   static Track::TypeInfo info{
      { "playable", "playable", XO("Playable Track") },
      false, &AudioTrack::ClassTypeInfo() };
   return info;
}

EnumSetting<SoloBehavior> TracksBehaviorsSolo{
   wxT("/GUI/Solo"),
   {
      ByColumns,
      { XO("Multi-track"), XO("Simple"),  XO("None") },
      { wxT("Multi"),     wxT("Simple"), wxT("None") }
   },
   0, // "Simple"
   { SoloBehaviorMulti, SoloBehaviorSimple, SoloBehaviorNone },
};
