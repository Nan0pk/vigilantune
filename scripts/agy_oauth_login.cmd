@echo off
cls
color 0E
title Antigravity OAuth Login
echo ===================================================
echo             ANTIGRAVITY OAUTH LOGIN
echo ===================================================
echo.
echo  [1/2] Backing up existing credentials...
if exist "%USERPROFILE%\.gemini\oauth_creds.json" (
    move /y "%USERPROFILE%\.gemini\oauth_creds.json" "%USERPROFILE%\.gemini\oauth_creds.json.bak" >nul
    echo  Backup created at:
    echo  %%USERPROFILE%%\.gemini\oauth_creds.json.bak
) else (
    echo  No existing credentials found to backup.
)
echo.
echo  [2/2] Launching Antigravity authentication flow...
echo  Please select "Sign in with Google" when prompted in the browser.
echo.
"C:\Users\Victus\AppData\Local\agy\bin\agy.exe"
echo.
echo ===================================================
echo  Authentication process finished.
echo ===================================================
pause
