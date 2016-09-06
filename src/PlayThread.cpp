#include "PlayThread.h"

/**
 * @file PlayThread.h
 * @brief Audio player implementation
 */

#include "Track.h"

#include <io/proxies/SfxInputProxy.h>
#include <stream_io/RtAudioOutput.h>
#include <RtAudio.h>
#include <QDebug>

PlayThread::PlayThread(QSettings* c) :
    QThread(0), options(c) {

    options->beginGroup("default");
    setThreshold(options->value("threshold").toInt());
    options->endGroup();
}

unsigned int PlayThread::getTracksCount() const {
    return tracks.size();
}

Track* PlayThread::getTrack(const unsigned int track){
    return (isValidTrack(track) ? tracks[track] : NULL);
}

int PlayThread::getActivatedTracks() const{
    int res = 0;
    for(int i= 0; i<tracks.size(); i++)
        res += (tracks[i]->isActivated() ? pow(2,i) : 0);

    return res;
}

int PlayThread::getThreshold() const {
    return m_threshold;
}

void PlayThread::run() {
    manager->input()->reset();
    bufferCount = 0;
    isPlaying = true;
    manager->execute();
}

void PlayThread::setMasterVolume(const unsigned int vol) {
    masterVolume->setGain(vol / 10.0);
}

void PlayThread::setVolume(const unsigned int track, const unsigned int vol) {
    if(isValidTrack(track))
        tracks[track]->setVolume(vol);
}

void PlayThread::setPan(const unsigned int track, const int pan) {
    if(isValidTrack(track))
        tracks[track]->setPan(pan);
}

void PlayThread::setMute(const unsigned int track, const bool doMute) {
    if(isValidTrack(track))
        tracks[track]->setMute(doMute);
}

void PlayThread::timeHandle() {
    if(++bufferCount > maxBufferCount)
        bufferCount = 0;

    emit actualBeatChanged(bufferCount * conf.bufferSize / double(conf.samplingRate));
}

void PlayThread::stop() {
    if(this->isRunning()){
        manager->stop();
        manager->input()->reset();
        bufferCount = 0;
        isPlaying = false;
    }
}

bool PlayThread::isStopped() const {
    return !isPlaying;
}

void PlayThread::reset() {
    options->beginGroup("default");
    setMasterVolume(options->value("master").toInt());
    options->endGroup();

    for(int i=0; i<tracks.size(); i++)
        tracks[i]->reset();
}

void PlayThread::solo(const unsigned int track, const bool state) {
    if(isValidTrack(track)) {
        tracks[track]->setSolo(state);

        if(state) {
            tracks[track]->setMute(false);

            for(int i=0; i<tracks.size(); i++)
                if(!tracks[i]->isSolo())
                    tracks[i]->setMute(true);
        } else {
            bool noMoreSolo = true;
            for(int i=0; i<tracks.size() && noMoreSolo; i++)
                if(tracks[i]->isSolo())
                    noMoreSolo = false;

            if(noMoreSolo)
                for(int i=0; i<tracks.size(); i++)
                    tracks[i]->setMute( !tracks[i]->isActivated() );
            else
                tracks[track]->setMute( true );
        }
    }
}

void PlayThread::switchBox(const unsigned int track) {
    if(isValidTrack(track))
        tracks[track]->setActivated(!tracks[track]->isActivated());
}

void PlayThread::setThreshold(const unsigned int threshold){
    m_threshold = (99 -  threshold) * 4 + 100;
}

void PlayThread::resetThreshold() {
    options->beginGroup("default");
    setThreshold(options->value("threshold").toInt());
    options->endGroup();
}

void PlayThread::load(const SongData& s) {
    // Reset to 0
    bufferCount = 0;
    int track_count = s.tracks.size();
    for(int i=0; i<tracks.size(); i++)
        delete tracks[i];
    tracks.clear();
    manager.reset();

    // Loading
    tracks.resize(track_count);
    std::vector<Input_p> chains(track_count);

#pragma omp parallel for
    for(int i = 0; i < track_count; i++) {
        Track* t = new Track(s.tracks[i], conf, options, i);
        connect(t, &Track::onActivationSwitch,
                this, &PlayThread::onEnablementChanged);
        tracks[i] = t;

        auto file = new FFMPEGFileInput<double>(tracks[i]->getFile(), conf);
        maxBufferCount = file->v(0).size() / conf.bufferSize;
        emit beatCountChanged(file->v(0).size() / double(conf.samplingRate));

        chains[i] = Input_p(new SfxInputProxy<double>(new StereoAdapter<double>(new LoopInputProxy<double>(file)),
                            new Sequence<double>(conf, tracks[i]->getVolumePtr(), tracks[i]->getPanPtr(), tracks[i]->getMutePtr() )));

        emit songLoaded(i+1,track_count);
    }

    // Master
    auto input = Input_p(new SfxInputProxy<double>(new SummationProxy<double>(new InputMultiplexer<double>(conf, chains)), masterVolume));

    // Manager
    manager = std::make_shared<StreamingManager<double>>(std::move(input),
              std::move(std::make_shared<RtAudioOutput<double>>(conf)),
              std::bind(&PlayThread::timeHandle, this),
                                                         conf);
}

bool PlayThread::isValidTrack(unsigned int track)
{
    bool isValid = track < getTracksCount();
    if(!isValid)
        qCritical() << tr("ERROR : Inexistent track") << track;

    return isValid;
}

void PlayThread::onEnablementChanged(bool enabled, int track) {
    emit muteChanged(track,
                     !enabled);
}
