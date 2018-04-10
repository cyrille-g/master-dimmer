void processSyncEvent (NTPSyncEvent_t ntpEvent) {
    if (ntpEvent) {
#ifdef WEB_DEBUG
  addLog("Time Sync error: ");
#endif
        if (ntpEvent == noResponse) {
#ifdef WEB_DEBUG
  addLog("NTP server not reachable");
#endif
        } else if (ntpEvent == invalidAddress) {
#ifdef WEB_DEBUG
  addLog("Invalid NTP server address");
#endif
        }
    } else {
#ifdef WEB_DEBUG
  sprintf(gLogBuffer,"Setting time to  %s and interval to 10mins / 1day",NTP.getTimeDateString(NTP.getLastNTPSync()).c_str());
  addLog(gLogBuffer);
#endif
    // set the system to do a sync every 86400 seconds when succesful, so once a day
    // if not successful, try syncing every 600s, so every 10 mins
    NTP.setInterval(600, 86400);

    }
}
