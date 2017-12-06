/*
  ==============================================================================

    PlayComponent.cpp
    Created: 11 Oct 2017 1:07:24pm
    Authors: Michael Castanieto
             Jonas Holfelt
             Gergely Csapo
              
  ==============================================================================
*/

#include "../JuceLibraryCode/JuceHeader.h"
#include "PlayComponent.h"
#include "Gesture.h"
#include "Mapper.h"
#include "AudioProcessorBundler.h"
#include <iostream>
#include <string>

//==============================================================================
PlayComponent::PlayComponent()
  // load background images
: impulseBackgroundImage(ImageFileFormat::loadFrom(BinaryData::drumbackdrop_png, (size_t) BinaryData::drumbackdrop_pngSize)),
  sustainBackgroundImage(ImageFileFormat::loadFrom(BinaryData::sustainedbackdrop_png, (size_t) BinaryData::sustainedbackdrop_pngSize)),
  // load button icon images
  impulseButtonIconImage(ImageFileFormat::loadFrom(BinaryData::drum_icon_png, (size_t) BinaryData::drum_icon_pngSize)),
  sustainButtonIconImage(ImageFileFormat::loadFrom(BinaryData::trumpet_icon_png, (size_t) BinaryData::trumpet_icon_pngSize)),
  discreteButtonIconImage(ImageFileFormat::loadFrom(BinaryData::discretetoggle_png, (size_t) BinaryData::discretetoggle_pngSize))
{

    isPlaying = false;
    Gesture::setCompWidth(getWidth());
    Gesture::setCompHeight(getHeight());
    
    //ToggleSpace buttons
    addAndMakeVisible (toggleSustain);
    toggleSustain.setImages(true, true, true,
                            sustainButtonIconImage, 0.5f, Colours::transparentBlack,
                            sustainButtonIconImage, 0.8f, Colours::transparentBlack,
                            sustainButtonIconImage, 1.0f, Colours::transparentBlack);    toggleSustain.setClickingTogglesState(true);
    toggleSustain.setToggleState(true, dontSendNotification);
    toggleSustain.addListener (this);
    
    addAndMakeVisible (toggleImpulse);
    toggleImpulse.setImages(true, true, true,
                            impulseButtonIconImage, 0.5f, Colours::transparentBlack,
                            impulseButtonIconImage, 0.8f, Colours::transparentBlack,
                            impulseButtonIconImage, 1.0f, Colours::transparentBlack);    toggleSustain.setClickingTogglesState(true);
    toggleImpulse.setClickingTogglesState(true);
    toggleImpulse.addListener (this);
    
    //Discrete toggle button
    addAndMakeVisible (toggleDiscrete);
    toggleDiscrete.setImages(true, true, true,
                            discreteButtonIconImage, 0.5f, Colours::transparentBlack,
                            discreteButtonIconImage, 0.8f, Colours::transparentBlack,
                            discreteButtonIconImage, 1.0f, Colours::transparentBlack);    toggleDiscrete.setClickingTogglesState(true);
    toggleDiscrete.setClickingTogglesState(true);
    toggleDiscrete.addListener (this);

    //Envelope setup
    ar = Envelope(44100, Envelope::AR);
    adsr = Envelope(44100, Envelope::ADSR);

    ar.isTriggered = &isPlaying;
    adsr.isTriggered = &isPlaying;
}

PlayComponent::~PlayComponent()
{
}

void PlayComponent::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    g.setColour (Colours::grey);
    g.drawRect (getLocalBounds(), 1);
    g.setColour (Colours::white);
    g.setFont (14.0f);

    if (isPlaying)
    {
      g.drawText ("Playing", getLocalBounds(),
                Justification::centred, true);
    }
    else
    {
      g.drawText ("Stopped", getLocalBounds(),
                Justification::centred, true);
    }

    if(toggleSpaceID == 1) //graphics for sustained space
    {
        g.drawImageWithin(sustainBackgroundImage, 0, 0, getWidth(), getHeight(), RectanglePlacement::stretchToFit); //set backdrop for sustained space
        if(discretePitchToggled)
            drawPitchBackDrop(g); //draws Pitchbar
        
        if(Gesture::getNumFingers() != 0) //draw ellipse on the users finger positions
        {
            for (int i = 0; i < Gesture::getNumFingers(); i++)
            {
                Gesture::drawPath(g, Gesture::getPath(i), i);
            }
        }
    }
    else if (toggleSpaceID == 2) //graphics for impulse space
    {
        g.drawImageWithin(impulseBackgroundImage, 0, 0, getWidth(), getHeight(), RectanglePlacement::onlyReduceInSize); //set backdrop for impulse space
        toggleDiscrete.setToggleState(false, dontSendNotification);
        discretePitchToggled = false;

        drawRipples(g);
    }
}

void PlayComponent::resized()
{
    int f = 7;
    Gesture::setCompWidth(getWidth());
    Gesture::setCompHeight(getHeight());
    toggleSustain.setBounds(getWidth()-(getWidth()/f + 5), 5, getWidth()/f, getWidth()/f);
    toggleImpulse.setBounds(getWidth()-(getWidth()/f + 5), getWidth()/f + 5, getWidth()/f, getWidth()/f);
    toggleDiscrete.setBounds(3,5,getWidth()/8,getWidth()/8);
}

void PlayComponent::mouseDown (const MouseEvent& e)
{
    Gesture::addFinger(e);
    mouseDrag(e);

    if(getToggleSpaceID() == 1) // note on
    {
        adsr.trigger(1);
    }
    if(getToggleSpaceID() == 2)
    {
        ar.trigger(1);
        addRipple();
    }

    initRead = true; // reset readIndex
    isPlaying = true;
      
    tapDetectCoords[0][0] = Gesture::getFingerPosition(0).x;
    tapDetectCoords[0][1] = Gesture::getFingerPosition(0).y;
    
    Mapper::setToggleSpace(toggleSpaceID);
}

void PlayComponent::mouseDrag (const MouseEvent& e)
{                      
    Gesture::updateFingers(e.source, e.source.getIndex());
  
    Gesture::setVelocity(Gesture::getFingerPosition(0).x, Gesture::getFingerPosition(0).y);

    Gesture::setAbsDistFromOrigin(Gesture::getFingerPosition(Gesture::getNumFingers()-1).x, Gesture::getFingerPosition(Gesture::getNumFingers()-1).y);
    
    if(discretePitchToggled && toggleSpaceID == 1)
    {
        Mapper::routeParameters(Gesture::getNumFingers(),true);
        Mapper::updateParameters();
    }
    else
    {
        Mapper::routeParameters(Gesture::getNumFingers(),false);
        Mapper::updateParameters();
    }
    
    fillCoordinates();
    tapDetectCoords[1][0] = Gesture::getFingerPosition(0).x;
    tapDetectCoords[1][1] = Gesture::getFingerPosition(0).y;
    
    rectNum = int(Gesture::getDiscretePitch()/2+6);
    
    repaint();
}

void PlayComponent::mouseUp (const MouseEvent& e)
{
    Gesture::setVelocityMax(Gesture::getVelocity());
    
    if(toggleSpaceID == 1)
        startRolloff();
    
    Gesture::rmFinger(e);

    if(Gesture::getNumFingers() == 0) // note off (initiate release) 
    {
        ar.trigger(0);
        adsr.trigger(0);
    }
    
    swipeEnd = true; // swipeEnd is a condition for resetting the buffer index when a new swipe is initiated
    Gesture::setResetPos(swipeEnd);
    
    Gesture::setTap(tapDetectCoords);

    rectNum = 12;
    
    repaint();
}

void PlayComponent::buttonClicked (Button* button)
{
    if(button == &toggleImpulse)
    {
        if(toggleImpulse.getToggleState()==0)
        {
            toggleImpulse.setToggleState(true, dontSendNotification);
        }
        else
        {
            toggleSustain.setToggleState(false, dontSendNotification);
            toggleSpaceID = 2;
        }
    }
    
    if(button == &toggleSustain)
    {
        if(toggleSustain.getToggleState()==0)
        {
            toggleSustain.setToggleState(true,dontSendNotification);
        }
        else
        {
            toggleImpulse.setToggleState(false, dontSendNotification);
            toggleSpaceID = 1;
        }
    }
    
    if(button == &toggleDiscrete)
    {
        if(toggleDiscrete.getToggleState()==0)
        {
            discretePitchToggled = false;
        }
        else
        {
            discretePitchToggled = true;
        }
    }
    repaint();
}

int PlayComponent::getToggleSpaceID()
{
    return toggleSpaceID;
}

void PlayComponent::timerCallback()
{
    if(toggleSpaceID == 1) // sustain
    {
        velocityRolloff *= 0.9;
        Gesture::setVelocity(velocityRolloff);
        
        if(velocityRolloff <= 0.03)
        {
            stopTimer();
        }
    }
    else // impulse
    {
        if(ripples.size() != 0)
        {
            for (int i = 0; i < ripples.size(); ++i)
            {
                if (ripples[i]->alpha < 0.1)
                    rmRipple(i);
            }
        }
        else
        {
            stopTimer();
        }
    }
    
    //NEED TO UPDATE PARAMETERS HERE FOR THE ROLLOFF TO AFFECT THE MAPPING
    //HOWEVER! If updateParameters() is called in a state where POSITION is mapped to something - CRASH
    //Mapper::routeParameters(0,false);
    //Mapper::updateParameters();

    repaint();
}

void PlayComponent::startRolloff()
{
    velocityRolloff = Gesture::getVelocity();
    startTimer(30);
}

void PlayComponent::fillCoordinates()
{
    //Fill the buffer for calculating direction, and calculate direction when the buffer reaches the end
    if (coordIndex > Gesture::directionBuffSize - 1)
    {
        Gesture::setDirection(coordinates);
        coordIndex = 0;
    }
    
    else if (swipeEnd)
    {
        //Start writing to the first index again and set all indexes equal to the first, to make sure that the deltaPosition is 0
        coordIndex = 0;
        
        for(int i = 0; i < Gesture::directionBuffSize; i++)
        {
            coordinates[i][0] = Gesture::getFingerPosition(0).x;
            coordinates[i][1] = Gesture::getFingerPosition(0).y;
        }
        
        swipeEnd = false;
    }
    
    else
    {
        coordinates[coordIndex][0] = Gesture::getFingerPosition(0).x;
        coordinates[coordIndex][1] = Gesture::getFingerPosition(0).y;
        coordIndex++;
    }
}

void PlayComponent::drawPitchBackDrop(Graphics& g)
{
    for (int i = 0; i < 12; i++)
    {
        rectListBackDrop.add(Rectangle<float>(-5,(getHeight()/12+0.5)*i,getWidth()+5,getHeight()/12+0.5));
    }
    
    for (int i = 0; i < 12; i++)
    {
        if(i%2 == 0)
        {
            g.setColour (Colours::darkgrey);
            g.setOpacity(0.1);
            g.fillRect(rectListBackDrop.getRectangle(i));
        }
        else
        {
            g.setColour (Colours::darkgrey);
            g.setOpacity(0.1);
            g.fillRect(rectListBackDrop.getRectangle(i));
        }
        
        g.setColour (Colours::white);
        g.setOpacity(0.1);
        g.drawRect(rectListBackDrop.getRectangle(i));
    }
    
    if(Gesture::getNumFingers() != 0)
    {
        if(discretePitchToggled)
        {
            g.setColour (Colours::lightgrey);
            g.setOpacity(0.5);
            g.fillRect(rectListBackDrop.getRectangle(11-rectNum));
            g.setColour (Colours::white);
            g.drawRect(rectListBackDrop.getRectangle(11-rectNum));
        }
    }
}

void PlayComponent::addRipple()
{
    int lastFingerIndex = Gesture::getNumFingers()-1;
    PlayComponent::Ripple* rip = new PlayComponent::Ripple(Gesture::getFingerPositionScreen(lastFingerIndex), ripples.size());
    ripples.add(rip);
    startTimerHz(60);
}

void PlayComponent::rmRipple(int i)
{
    ripples.removeObject(ripples[i]);
}

void PlayComponent::drawRipples(Graphics& g)
{
    if(ripples.size() != 0)
    {
        for (int i = 0; i < ripples.size(); ++i) // fingers
        {
            g.setOpacity(ripples[i]->alpha);
            g.drawEllipse(ripples[i]->pos.x-ripples[i]->circleSize/2, ripples[i]->pos.y-ripples[i]->circleSize/2, ripples[i]->circleSize, ripples[i]->circleSize, ripples[i]->line);
    
            ripples[i]->circleSize += ripples[i]->acc*=0.98; // increase circle radii
            ripples[i]->alpha -= 0.05; // 0.05 + ar.getAmplitude()*0.9; // decrease opacity
            if(ripples[i]->line > 0.2)
                ripples[i]->line -= 0.4; // decrease line thickness
            else
                ripples[i]->line = 0;  
        }
    }
}
