/* -------------------------------- DEBUG FUNCTIONS ---------------------------*/
void addLog(char *pLog) {
  while (gWeblogCount >= WEBLOG_QUEUE_SIZE) {
    weblogQueueElem_t *pOldLogelem = STAILQ_FIRST(&gLogQueue);
    STAILQ_REMOVE_HEAD(&gLogQueue, logEntry);
    free(pOldLogelem->pLogLine);
    free(pOldLogelem);
    gWeblogCount--;
  }
  weblogQueueElem_t *pNewLogElem = (weblogQueueElem_t *) malloc (sizeof (weblogQueueElem_t));
  char *pLogLine = (char *) malloc (strlen(pLog) + 1);
  strcpy(pLogLine, pLog);
  pNewLogElem->pLogLine = pLogLine;
  STAILQ_INSERT_TAIL(&gLogQueue, pNewLogElem, logEntry);
  gWeblogCount++;
}

void extractLog(String *pOutput) {
  pOutput->concat("\n\nLogs:\n ");
  weblogQueueElem_t *pLogElem = NULL;
  STAILQ_FOREACH(pLogElem, &gLogQueue, logEntry) {
     pOutput->concat(pLogElem->pLogLine);
     pOutput->concat("\n ");
  }
}

