void scenarioQueueSetup(void) {

  // Init scenario queues
  int index = 0;
  size_t topicSize = 0;
  Serial.println("MQTT set topics");
  for (index = 0; index < MAX_ROOM_COUNT; index++) {
    STAILQ_INIT(&(gLedStrip[index].transitionQueue));
    /* init MQTT set and state topic: get the size first. do not forget trailing \0 and the 2 slashes */
    topicSize = strlen (MQTT_PREFIX) + strlen (LUT_IndexToRoomName[index]) + strlen (MQTT_STATE) + 3;
    gLedStrip[index].stateTopic = (char *)malloc(topicSize);
    sprintf(gLedStrip[index].stateTopic, "%s/%s/%s", MQTT_PREFIX, LUT_IndexToRoomName[index], MQTT_STATE);

    topicSize = strlen (MQTT_PREFIX) + strlen (LUT_IndexToRoomName[index]) + strlen (MQTT_SET) + 3;
    gLedStrip[index].setTopic = (char *)malloc(topicSize);
    sprintf(gLedStrip[index].setTopic, "%s/%s/%s", MQTT_PREFIX, LUT_IndexToRoomName[index], MQTT_SET);
#ifdef SERIAL_DEBUG
    Serial.println(gLedStrip[index].setTopic);
#endif
  }
}


/********************************** JSON PROCESSOR *****************************************/
bool processJson(char* message, uint8_t index) {
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(message);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return false;
  }

  uint16_t targetBrightness = 0;
  uint16_t transitionTimeMs = MIN_TRANSITION_TIME ;
  uint8_t  transitionPreset = MANUAL;

  /* The "state" command is self sufficient. If it is present,
     there is no need to parse anything else.
     Other data is relevant when state is not in the JSON . We still need to set
     targetBrightness, transitionTimeMs and transitionPreset */

  const char* res = root["state"];
  if (res) {
    if (strcmp(root["state"], gConfOnCommand) == 0 ) {
#ifdef SERIAL_DEBUG      
      Serial.println("State set ON");
#endif      
      targetBrightness = MAX_BRIGHTNESS_ALLOWED;
      transitionPreset = CLOCK_BASED;
    } else {
#ifdef SERIAL_DEBUG            
      Serial.println("State set and not ON, setting as OFF");
#endif
      transitionPreset = CLOCK_BASED;
      targetBrightness = 0;
    }
  } else { // if there is no STATE in the JSON
    res = root["transition"]["timeMs"];
    if (!res) {
#ifdef WEB_DEBUG
      addLog("No transition time set");
#endif
#ifdef SERIAL_DEBUG      
      Serial.println("No transition time set");
#endif
      transitionTimeMs = MIN_TRANSITION_TIME;
    } else {
      if (root["transition"]["timeMs"] <= MIN_TRANSITION_TIME) {
#ifdef SERIAL_DEBUG      
        Serial.println("Transition too short, setting to lower limit");
#endif        
#ifdef WEB_DEBUG        
        addLog("transition time too short");
#endif        
        transitionTimeMs = MIN_TRANSITION_TIME;
      } else if (root["transition"]["timeMs"] > MAX_TRANSITION_TIME) {
#ifdef SERIAL_DEBUG      
        Serial.println("Transition too short high, setting to higher limit");
#endif
#ifdef WEB_DEBUG        
          addLog("transition too long");
#endif
        transitionTimeMs = MAX_TRANSITION_TIME;
      
      } else {
#ifdef WEB_DEBUG   
        addLog("regular transition time found");
#endif        
        transitionTimeMs = root["transition"]["timeMs"];
      }
    }
    /* is there a preset ? */
    res = root["transition"]["preset"];
    if (res) {
      if (strcmp(root["transition"]["preset"], "CLOCK") == 0) {
        transitionPreset = CLOCK_BASED;
#ifdef SERIAL_DEBUG              
        Serial.println("Transition clock based");
#endif
      } else if (strcmp(root["transition"]["preset"], "LIGHT") == 0) {
#ifdef SERIAL_DEBUG      
        Serial.println("Transition light sensor based");
#endif        
        transitionPreset = LIGHT_BASED;
      } else if (strcmp(root["transition"]["preset"], "AUTO") == 0) {
#ifdef SERIAL_DEBUG      
        Serial.println("Transition both clock and light sensor based");
#endif        
        transitionPreset = CLOCK_AND_LIGHT_BASED;
      } else {
#ifdef SERIAL_DEBUG              
        Serial.println("Transition set to manual");
#endif     
        transitionPreset = MANUAL;
      }
    } else {
#ifdef SERIAL_DEBUG            
      Serial.println("Transition not specified");
#endif
      transitionPreset = MANUAL;
    }
  } // if there is transition in the JSON

  /* is there a brightness directive ? */
  res = root["brightness"];
  if (res) {
    if (root["brightness"] > MAX_BRIGHTNESS_ALLOWED) {
#ifdef SERIAL_DEBUG      
      Serial.println("Brightness set too high, limiting");
#endif
      targetBrightness = MAX_BRIGHTNESS_ALLOWED;
    } else {
      targetBrightness = root["brightness"];
    }
  } else { // if there is no brightness, set it to 0
#ifdef SERIAL_DEBUG      
    Serial.println("No brightness specified, set to 0");
#endif
targetBrightness = 0;
  } // if there is no brightness in the json

  //build and add the scenario

//  if (!gclockswitch) {
    buildAndInsertScenario(targetBrightness, transitionTimeMs, CLOCK_BASED, index);
//    gclockswitch = true;
//  } else {
//    buildAndInsertScenario(targetBrightness, transitionTimeMs, transitionPreset, index);
//  }
return true;
}

/***************************** empty a scenario queue including the playing one ***************************************/
void emptyScenarioQueue(uint8_t index) {
 // free the data in the queue
  transitionQueueElem_t *pTransition = NULL;
  STAILQ_FOREACH(pTransition, &(gLedStrip[index].transitionQueue), transitionEntry) {
    STAILQ_REMOVE_HEAD(&(gLedStrip[index].transitionQueue), transitionEntry);
    free(pTransition);
  } 
}

/***************************** linear scenario builder *******************************************/
transitionQueueElem_t * buildLinearScenario (int16_t startingBrightness, int16_t targetBrightness, uint16_t transitionTimeMs) {


#ifdef WEB_DEBUG
   sprintf(gLogBuffer,"linearscenario %d %d %d",
            startingBrightness, targetBrightness,transitionTimeMs);
  addLog(gLogBuffer);
#endif

  // we want the max stepCount we can, so it depends wether transition time
  // or brightness count is higher.
  // we might want to set a minimum delay or minimum brightness step
  // so account for that
  transitionQueueElem_t *pTransition = (transitionQueueElem_t *)malloc(sizeof(transitionQueueElem_t));
    if (pTransition == NULL) {
#ifdef WEB_DEBUG
  addLog("Transition malloced NULL");      
#endif
      return NULL;
    }
  if ((abs(targetBrightness - startingBrightness) / MIN_STEP_BRIGHTNESS) > (transitionTimeMs / MIN_STEP_DELAY)) {
    //we can do more than 1 brightness step per delay step ms. set stepDelay to that time
    pTransition->stepDelay = MIN_STEP_DELAY;
    pTransition->stepCount = transitionTimeMs / pTransition->stepDelay;
    pTransition->stepBrightness = (targetBrightness - startingBrightness) / pTransition->stepCount;
  } else {
    //we cannot. brightness difference is the limiting factor.
    pTransition->stepCount = abs(targetBrightness - startingBrightness) / MIN_STEP_BRIGHTNESS;
    pTransition->stepDelay = transitionTimeMs / pTransition->stepCount ;
    pTransition->stepBrightness = (targetBrightness > startingBrightness) ? MIN_STEP_BRIGHTNESS : -MIN_STEP_BRIGHTNESS;

  }
  /* whichever, save target brightness in the transition */
  pTransition->targetBrightness = targetBrightness;

#ifdef WEB_DEBUG
   sprintf(gLogBuffer,"res %d %d %d %u",
            pTransition->stepDelay, pTransition->stepCount,pTransition->stepBrightness,pTransition->targetBrightness);
  addLog(gLogBuffer);
#endif
  return pTransition;
}

/***************************** decreasing luminosity scenario building ***************************************/
transitionQueueElem_t * buildDecreaseLuminosityScenario (int16_t startingBrightness, uint16_t timeSinceStart)
{
  #ifdef WEB_DEBUG
  sprintf(gLogBuffer,"decLumScenario start br %d tstart %u",
           startingBrightness, timeSinceStart);
  addLog(gLogBuffer);
  #endif

  float timeRatio = timeSinceStart / (gConfStopLoweringLightOffset * 60.0);
  int32_t tmpTargetBrightness = 0;
  uint32_t tmpTransitionTimeMs = 0;
  int16_t  targetBrightness = 0;
  uint16_t transitionTimeMs = 0;
  /* this is the increase light scenario. Since we are clock based, 
  we do not receive target brightness or anything. what we know is if the light was already on
  we need to turn it off and vice versa */
  if (startingBrightness == 0)
  { 
    tmpTargetBrightness = (1.0 - timeRatio) * (MAX_BRIGHTNESS_ALLOWED - gConfTargetLowestBrightness)  + gConfTargetLowestBrightness;
    tmpTransitionTimeMs =timeRatio * (gConfTargetLongestTransitionTime - MIN_TRANSITION_TIME)  + MIN_TRANSITION_TIME; ;
    targetBrightness = (int16_t)tmpTargetBrightness;
    transitionTimeMs = (uint16_t)tmpTransitionTimeMs;
  } else {
    tmpTransitionTimeMs =(1.0 - timeRatio) * (gConfTargetLongestTransitionTime - MIN_TRANSITION_TIME)  + MIN_TRANSITION_TIME; ;
    targetBrightness = 0;
    transitionTimeMs = (uint16_t)tmpTransitionTimeMs;
  }
  
  if (startingBrightness == tmpTargetBrightness) {
  #ifdef WEB_DEBUG
    addLog("brightnesses are the same, change nothing");
  #endif
      return NULL;    
    }

#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"decLumScenario start br %d target br %d transition time %u",
           startingBrightness, targetBrightness, transitionTimeMs);
  addLog(gLogBuffer);
#endif
  return buildLinearScenario (startingBrightness, targetBrightness,  transitionTimeMs);}

/***************************** increasing luminosity scenario building ***************************************/
transitionQueueElem_t * buildIncreaseLuminosityScenario (int16_t startingBrightness, uint16_t timeSinceStart)
{
  #ifdef WEB_DEBUG
  sprintf(gLogBuffer,"incLumScenario start br %d tstart %u",
           startingBrightness, timeSinceStart);
  addLog(gLogBuffer);
  #endif

  float timeRatio = timeSinceStart / (gConfStopDimmingOffset * 60.0);
  int32_t tmpTargetBrightness = 0;
  uint32_t tmpTransitionTimeMs = 0;
  int16_t  targetBrightness = 0;
  uint16_t transitionTimeMs = 0;
  /* this is the increase light scenario. Since we are clock based, 
  we do not receive target brightness or anything. what we know is if the light was already on
  we need to turn it off and vice versa */
  if (startingBrightness == 0)
  { 
    tmpTargetBrightness = (timeRatio * (MAX_BRIGHTNESS_ALLOWED - gConfTargetLowestBrightness))  + gConfTargetLowestBrightness;
    tmpTransitionTimeMs =(1.0 - timeRatio) * (gConfTargetLongestTransitionTime - MIN_TRANSITION_TIME)  + MIN_TRANSITION_TIME; ;
    targetBrightness = (int16_t)tmpTargetBrightness;
    transitionTimeMs = (uint16_t)tmpTransitionTimeMs;
  } else {
    tmpTransitionTimeMs =(1.0 - timeRatio) * (gConfTargetLongestTransitionTime - MIN_TRANSITION_TIME)  + MIN_TRANSITION_TIME; ;
    targetBrightness = 0;
    transitionTimeMs = (uint16_t)tmpTransitionTimeMs;
  }
  
  if (startingBrightness == tmpTargetBrightness) {
  #ifdef WEB_DEBUG
    addLog("brightnesses are the same, change nothing");
  #endif
      return NULL;    
    }

#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"incLumScenario start br %d target br %d transition time %u",
           startingBrightness, targetBrightness, transitionTimeMs);
  addLog(gLogBuffer);
#endif
  return buildLinearScenario (startingBrightness, targetBrightness,  transitionTimeMs);
}

/***************************** time interval detection ***************************************/

uint16_t MinutesSinceStartOfInterval(uint8_t currentHour, uint8_t currentMinute, uint8_t startHour, uint8_t intervalSize)
{
  uint16_t timeSinceStart = 0;  

  if (startHour + intervalSize > 23 )  {
    /* the interval includes 0 */
    if ((currentHour >= startHour) && (currentHour < 0)) {
      /* first part of the clock */
      timeSinceStart =  ((currentHour - startHour ) * 60) + currentMinute;
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"%d:%d between %d and 0h. interval %d tStart %d",
      currentHour,currentMinute,startHour,intervalSize,timeSinceStart);
  addLog(gLogBuffer);
#endif
    } else if ((currentHour >=0) && (currentHour < (startHour + intervalSize - 24))) {
      /* second part of the clock */
      timeSinceStart =  ((24 + currentHour - startHour  ) * 60) + currentMinute;
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"%d:%d between 0h and max time. interval %d tStart %d",
      currentHour,currentMinute,startHour,intervalSize,timeSinceStart);
  addLog(gLogBuffer);
#endif
    } else {
      /* not in the interval, do nothing */
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"%d:%d not in the interval",currentHour,currentMinute);
  addLog(gLogBuffer);
#endif
    }
  } else if ((currentHour>=startHour) && (currentHour < (startHour + intervalSize))) {
  /* the interval does not include 0 */
  timeSinceStart =  ((currentHour - startHour ) * 60) + currentMinute;
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"%d:%d is between %d and interval %d. tStart %d",
      currentHour,currentMinute,startHour,intervalSize,timeSinceStart);
  addLog(gLogBuffer);
#endif
  } else {
    /* out of interval, value is 0 so do nothing */
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"%d:%d not in the interval",currentHour,currentMinute);
  addLog(gLogBuffer);
#endif
  }
  return timeSinceStart;
}

/****************** accessor *******************************************/
transitionQueueElem_t *getScenarioStart(uint8_t index, int16_t *startingBrightness)
{
  transitionQueueElem_t *pBaseState = NULL;
    if (!STAILQ_EMPTY(&(gLedStrip[index].transitionQueue))) {
    /* yes, load the last inserted as reference */
    // does not work right now, missing implementation of __offsetof in arduino core 3.2.0
    // workaround since i know the structure size
#define  MY_STAILQ_LAST(head, type)          \
    (STAILQ_EMPTY((head)) ?           \
     NULL :             \
     ((struct type *)(void *)       \
((char *)((head)->stqh_last) - 8)))
    
    pBaseState = MY_STAILQ_LAST(&(gLedStrip[index].transitionQueue),transitionQueueElem_s);
    
    *startingBrightness = pBaseState->targetBrightness;
  } else if (gpCurrentTransition[index] != NULL) {
      /* yes, use the current playing scenario target */
     pBaseState = gpCurrentTransition[index];
     *startingBrightness = pBaseState->targetBrightness;
  } else {
    /* nothing playing. Use current */
    *startingBrightness = gLedStrip[index].currentBrightness;
  }
  
  /* clear any remaining or playing transition */
  if (gpCurrentTransition[index] != NULL) {
    free (gpCurrentTransition[index]);
    gpCurrentTransition[index] = NULL;
  }
  emptyScenarioQueue(index);

}

/****************** scenario builder from JSON *******************************************/
void buildAndInsertScenario (int16_t targetBrightness, uint16_t transitionTimeMs, uint8_t transitionPreset, uint8_t index)
{

#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"scenario: br from %d to %d tr time %d preset %d room index %d",
      gLedStrip[index].currentBrightness, targetBrightness, transitionTimeMs,transitionPreset, index);
  addLog(gLogBuffer);
#endif

#ifdef SERIAL_DEBUG
  Serial.print("Scenario fading from ");
  Serial.print(gLedStrip[index].currentBrightness, DEC);
  Serial.print(" to ");
  Serial.print(targetBrightness, DEC);
  Serial.print(" in ");
  Serial.print(transitionTimeMs, DEC);
  Serial.print(" ms with preset: ");
  Serial.println(transitionPreset, DEC);
#endif

  /* first find where to start. 
   *  If there is a transition in the queue use the last inserted;
   *  If there is none, check if there is a transition playing;
   *  If there is none either, use the current state
   */
  int16_t startingBrightness = 0;
  transitionQueueElem_t *pBaseState = getScenarioStart(index, &startingBrightness);

  /* check if there is a transition to do */
  if (targetBrightness == startingBrightness) {
  /* none ! So do nothing */
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"Target brightness = starting brightness, no scenario");
  addLog(gLogBuffer);
#endif
     return ;
  }
  transitionQueueElem_t *pTransition=NULL;
  
   if (transitionPreset == CLOCK_BASED) {
     /* this is the clock based mode */
      /* in clock based mode, the more it gets near a set hour,
        the dimmer the light,
        the longer the transition */
        
      uint8_t currentHour = hour();
      uint8_t currentMinute = minute();
  
  #ifdef WEB_DEBUG
    sprintf(gLogBuffer,"CLOCK_BASED: currentHour %d currentMinute %d",
        currentHour, currentMinute);
    addLog(gLogBuffer);
  #endif
      /* time sections are like this:
       *  gConfAutoDimStartHour | gConfStopLoweringLightOffset | gConfStartIncreasingLightOffset | gConfStopDimmingOffset
       *  basically, the light output will do this:  \_______/
       *  
       *  the starting point of dimming is the first variable
       *  the starting point of lowest light is first variable + the second
       *  the starting point of less dimming is first variable + the second + the third
       *  and we go back to standard dimming when time reached the first variable + second + third + fourth
       *  
       *  working with offset is way simpler in terms of rollover.
       */
      uint16_t timeSinceInterval =  MinutesSinceStartOfInterval(currentHour, currentMinute, 
                                                                gConfAutoDimStartHour, gConfStopLoweringLightOffset);
  
      if (timeSinceInterval > 0 ) {
          pTransition = buildDecreaseLuminosityScenario(startingBrightness, timeSinceInterval); 
      } else {
        timeSinceInterval =  MinutesSinceStartOfInterval(currentHour, currentMinute, 
                                                        (gConfAutoDimStartHour + gConfStopLoweringLightOffset) % 24,
                                                        gConfStartIncreasingLightOffset);
        if (timeSinceInterval > 0 ) {
        /* this is the darkest light scenario.
        Buid the scenario with the lowest values */
          pTransition = buildLinearScenario(startingBrightness, gConfTargetLowestBrightness, gConfTargetLongestTransitionTime);           
        } else {
          timeSinceInterval =  MinutesSinceStartOfInterval(currentHour, currentMinute, 
                                                          (gConfAutoDimStartHour + gConfStopLoweringLightOffset + gConfStopLoweringLightOffset) % 24,
                                                          gConfStopDimmingOffset);
          if (timeSinceInterval > 0)  {
            /* this is the increasing light scenario */
#ifdef WEB_DEBUG
    sprintf(gLogBuffer,"buildIncreaseLuminosityScenario: timeSinceInterval %d",
        timeSinceInterval);
    addLog(gLogBuffer);
#endif             
             pTransition = buildIncreaseLuminosityScenario(startingBrightness, timeSinceInterval);
          } else  {
          /* nothing special is happening. This is "no dimming" time, build linear scenario */
          pTransition = buildLinearScenario(startingBrightness, targetBrightness, transitionTimeMs);
        }
      }
      }
// TODO : code these modes, if ever 
//   } else if (transitionPreset == LIGHT_BASED) {
//   /* this is the light based mode */
//      pTransition = buildLinearScenario(startingBrightness, targetBrightness, transitionTimeMs);
//   } else if (transitionPreset == CLOCK_AND_LIGHT_BASED) {
//   /*  this is the clock and light based mode */
//      pTransition = buildLinearScenario(startingBrightness, targetBrightness, transitionTimeMs);
   } else {
  /* this is the manual mode */
     pTransition = buildLinearScenario(startingBrightness,targetBrightness, transitionTimeMs);
  }
  
  if (pTransition!=NULL){
    STAILQ_INSERT_TAIL(&(gLedStrip[index].transitionQueue), pTransition, transitionEntry);
    #ifdef SERIAL_DEBUG
    Serial.print("Target brightness ");
    Serial.print(pTransition->targetBrightness, DEC);
    Serial.print("delay step ");
    Serial.print(pTransition->stepDelay, DEC);
    Serial.print(", step count ");
    Serial.print(pTransition->stepCount, DEC);
    Serial.print(", brightness step ");
    Serial.println(pTransition->stepBrightness, DEC);
    #endif
    #ifdef WEB_DEBUG
    sprintf(gLogBuffer,"Target brightness %d delay step %d, step count %d, brightness step %d",
      pTransition->targetBrightness, pTransition->stepDelay, pTransition->stepCount, pTransition->stepBrightness);
    addLog(gLogBuffer);
    #endif
  } else {
  #ifdef WEB_DEBUG
    addLog("Transition NULL");
  #endif
  }
}
