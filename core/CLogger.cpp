#include "CLogger.h"
#include "systems/Librarysys.h"

CLogger g_Logger;

void CLogger::_NewMapFile()
{
	if (!m_Active)
	{
		return;
	}
	
	char _filename[256];
	int i = 0;

	time_t t;
	time(&t);
	tm *curtime = localtime(&t);

	while (true)
	{
		g_LibSys.PathFormat(_filename, sizeof(_filename), "%s/logs/L%02d%02d%03d.log", g_SourceMod.GetSMBaseDir(), curtime->tm_mon + 1, curtime->tm_mday, i);
		FILE *fp = fopen(_filename, "r");
		if (!fp)
		{
			break;
		}
		fclose(fp);
		i++;
	}
	m_NrmFileName.assign(_filename);

	FILE *fp = fopen(m_NrmFileName.c_str(), "w");
	if (!fp)
	{
		g_SMAPI->ConPrint("[SM] Unexpected fatal logging error. SourceMod logging disabled.\n");
		m_Active = false;
	} else {
		char date[32];
		strftime(date, sizeof(date), "%m/%d/%Y - %H:%M:%S", curtime);
		fprintf(fp, "L %s: SourceMod log file started (file \"L%02d%02d%03d.log\") (Version \"%s\")\n", date, curtime->tm_mon + 1, curtime->tm_mday, i, SOURCEMOD_VERSION);
		fprintf(fp, "L %s: Info (map \"%s\")\n", date, m_CurMapName.c_str());
		fclose(fp);
	}
}

void CLogger::_CloseFile()
{
	if (!m_Active)
	{
		return;
	}

	FILE *fp = NULL;
	if (!m_NrmFileName.empty())
	{
		fp = fopen(m_NrmFileName.c_str(), "r+");
		if (fp)
		{
			fseek(fp, 0, SEEK_END);
			LogMessage("Log file closed.");
			fclose(fp);
		}
		m_NrmFileName.clear();
	}

	if (!m_ErrMapStart)
	{
		return;
	}
	fp = fopen(m_ErrFileName.c_str(), "r+");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		LogMessageEx(LogType_Error, "Error log file session closed.");
		fclose(fp);
	}
	m_ErrFileName.clear();
}

void CLogger::InitLogger(LoggingMode mode, bool startlogging)
{
	m_mode = mode;
	m_Active = startlogging;

	time_t t;
	time(&t);
	tm *curtime = localtime(&t);
	m_CurDay = curtime->tm_mday;

	char _filename[256];
	g_LibSys.PathFormat(_filename, sizeof(_filename), "%s/logs/errors_%02d%02d%02d.log", g_SourceMod.GetSMBaseDir(), curtime->tm_mon + 1, curtime->tm_mday, curtime->tm_year - 100);
	m_ErrFileName.assign(_filename);

	switch (m_mode)
	{
	case LoggingMode_PerMap:
		{
			if (!startlogging)
			{
				m_DelayedStart = true;
			}
			break;
		}
	case LoggingMode_Daily:
		{
			g_LibSys.PathFormat(_filename, sizeof(_filename), "%s/logs/L%02d%02d.log", g_SourceMod.GetSMBaseDir(), curtime->tm_mon + 1, curtime->tm_mday);
			m_NrmFileName.assign(_filename);
			m_DailyPrintHdr = true;
			break;
		}
	}
}

void CLogger::CloseLogger()
{
	_CloseFile();
}

void CLogger::LogMessage(const char *vafmt, ...)
{
	if (!m_Active)
	{
		return;
	}

	if (m_mode == LoggingMode_HL2)
	{
		_PrintToHL2Log(vafmt);
		return;
	}

	if (m_DelayedStart)
	{
		m_DelayedStart = false;
		_NewMapFile();
	}

	static char msg[3072];
	va_list ap;
	va_start(ap, vafmt);
	vsnprintf(msg, sizeof(msg), vafmt, ap);
	va_end(ap);

	char date[32];
	time_t t;
	time(&t);
	tm *curtime = localtime(&t);
	strftime(date, sizeof(date), "%m/%d/%Y - %H:%M:%S", curtime);

	FILE *fp = NULL;
	if (m_mode == LoggingMode_PerMap)
	{
		fp = fopen(m_NrmFileName.c_str(), "a+");
		if (!fp)
		{
			_NewMapFile();
			fp = fopen(m_NrmFileName.c_str(), "a+");
			if (!fp)
			{
				goto print_error;
			}
		}
	} else {
		if (m_CurDay != curtime->tm_mday)
		{
			char _filename[256];
			g_LibSys.PathFormat(_filename, sizeof(_filename), "%s/logs/L%02d%02d.log", g_SourceMod.GetSMBaseDir(), curtime->tm_mon + 1, curtime->tm_mday);
			m_NrmFileName.assign(_filename);
			m_CurDay = curtime->tm_mday;
			m_DailyPrintHdr = true;
		}
		fp = fopen(m_NrmFileName.c_str(), "a+");
	}

	if (fp)
	{
		if (m_DailyPrintHdr)
		{
			m_DailyPrintHdr = false;
			fprintf(fp, "L %s: SourceMod log file started (file \"L%02d%02d.log\") (Version \"%s\")\n", date, curtime->tm_mon + 1, curtime->tm_mday, SOURCEMOD_VERSION);
		}
		fprintf(fp, "L %s: %s\n", date, msg);
		fclose(fp);
	} else {
		goto print_error;
	}

	g_SMAPI->ConPrintf("L %s: %s\n", date, msg);
print_error:
	g_SMAPI->ConPrint("[SM] Unexpected fatal logging error. SourceMod logging disabled.\n");
	m_Active = false;
}

void CLogger::LogMessageEx(LogType type, const char *vafmt, ...)
{
	if (!m_Active)
	{
		return;
	}

	if (type == LogType_Normal)
	{
		LogMessage(vafmt);
		return;
	}

	time_t t;
	time(&t);
	tm *curtime = localtime(&t);

	char date[32];
	strftime(date, sizeof(date), "%m/%d/%Y - %H:%M:%S", curtime);

	if (curtime->tm_mday != m_CurDay)
	{
		char _filename[256];
		g_LibSys.PathFormat(_filename, sizeof(_filename), "%s/logs/errors_%02d%02d%02d.log", g_SourceMod.GetSMBaseDir(), curtime->tm_mon + 1, curtime->tm_mday, curtime->tm_year - 100);
		m_ErrFileName.assign(_filename);
		m_CurDay = curtime->tm_mday;
		m_ErrMapStart = false;
	}

	static char msg[3072];
	va_list ap;
	va_start(ap, vafmt);
	vsnprintf(msg, sizeof(msg), vafmt, ap);
	va_end(ap);

	FILE *fp = fopen(m_ErrFileName.c_str(), "a+");
	if (fp)
	{
		if (!m_ErrMapStart)
		{
			fprintf(fp, "L %s: SourceMod error session started\n", date);
			fprintf(fp, "L %s: Info (map \"%s\") (log file \"errors_%02d%02d%02d.log\")\n", date, m_CurMapName.c_str(), curtime->tm_mon + 1, curtime->tm_mday, curtime->tm_year - 100);
			m_ErrMapStart = true;
		}
		fprintf(fp, "L %s: %s\n", date, msg);
		fclose(fp);
	} else {
		g_SMAPI->ConPrint("[SM] Unexpected fatal logging error. SourceMod logging disabled.\n");
		m_Active = false;
	}

	g_SMAPI->ConPrintf("L %s: %s\n", date, msg);
}

void CLogger::MapChange(const char *mapname)
{
	m_CurMapName.assign(mapname);

	switch (m_mode)
	{
	case LoggingMode_Daily:
		{
			LogMessage("-------- Mapchange to %s --------", mapname);
			break;
		}
	case LoggingMode_PerMap:
		{
			_NewMapFile();
			break;
		}
	}

	if (m_ErrMapStart)
	{
		LogMessageEx(LogType_Error, "Error log file session closed.");
	}
	m_ErrMapStart = false;
}

void CLogger::_PrintToHL2Log(const char *fmt, ...)
{
	static char msg[3072];
	size_t len;

	va_list ap;
	va_start(ap, fmt);
	len = vsnprintf(msg, sizeof(msg)-1, fmt, ap);
	msg[len++] = '\n';
	msg[len] = '\0';
	va_end(ap);

	engine->LogPrint(msg);
}