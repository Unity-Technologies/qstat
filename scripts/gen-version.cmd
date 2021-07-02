@echo off

:: Default file paramters
SETLOCAL EnableDelayedExpansion
SET "VERSION_FILE=.version"
SET "TEMPLATE_FILE=version.h.tmpl"
SET "HEADER_FILE=version.h"

IF NOT [%1] == [] (
	SET "VERSION_FILE=%1"
)
IF NOT [%2] == [] (
	SET "TEMPLATE_FILE=%2"
)
IF NOT [%3] == [] (
	SET "HEADER_FILE=%3"
)

IF [!QSTAT_VERSION!] == [] (
	:: QSTAT_VERSION not set determine from git.
	FOR /F "usebackq" %%v IN (`cmd /c "git status --porcelain | grep -Fv 'gnuconfig.h.in~' | grep -E '^(M^| M^|\?\?)' | wc -l | sed -e 's/^[[:space:]]*//'"`) DO (
		IF NOT %%v == 0 (
			:: We have modifications so indicate so in the version.
			SET "QSTAT_VERSION=!QSTAT_VERSION!-modified"
		)
	)
)

IF EXIST "!VERSION_FILE!" (
	:: .version file exists load the value.
	set /p OLD_VERSION=<"!VERSION_FILE!"
)

IF NOT [!OLD_VERSION!] == [!QSTAT_VERSION!] (
	:: version has changed update the file.
	echo|set /p="!QSTAT_VERSION!" > "!VERSION_FILE!"
)

:: Update version.h if needed.
IF NOT EXIST "!HEADER_FILE!" (
	:: File doesn't exist to just create.
	sed "s/CHANGEME/!QSTAT_VERSION!/g" "!TEMPLATE_FILE!" > "!HEADER_FILE!"
) else (
	:: File exists compare with new.
	SET "TMP_FILE=!HEADER_FILE!.tmp"

	sed "s/CHANGEME/!QSTAT_VERSION!/g" "!TEMPLATE_FILE!" > "!TMP_FILE!"

	:: cmp doesn't correctly set ERRORLEVEL so we use redirection.
	cmp -s "!HEADER_FILE!" "!TMP_FILE!" && rm -f "!TMP_FILE!" || mv "!TMP_FILE!" "!HEADER_FILE!"
)

ENDLOCAL
