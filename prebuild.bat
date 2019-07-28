@echo off
:: Turn symlinks into plain files
::

:: libel\protocol\el.proto -> .submodules\embedded-lib-protocol\el.proto
DEL /AS libel\protocol\el.proto >nul 2>nul && COPY .submodules\embedded-lib-protocol\el.proto libel\protocol\el.proto >nul

:: sample_client_synergy\config.h
COPY sample_client\config.h sample_client_synergy\config.h >nul

:: sample_client_synergy\Makefile
COPY sample_client\Makefile sample_client_synergy\Makefile >nul

:: sample_client_synergy\sample_client.conf
COPY sample_client\sample_client.conf sample_client_synergy\sample_client.conf >nul

:: sample_client_synergy\protocol\el.options
COPY libel\protocol\el.options sample_client_synergy\protocol\el.options >nul

:: sample_client_synergy\protocol\el.proto
COPY .submodules\embedded-lib-protocol\el.proto sample_client_synergy\protocol\el.proto >nul

:: sample_client_synergy\protocol\Makefile
COPY libel\protocol\Makefile sample_client_synergy\protocol\Makefile >nul

:: sample_client_synergy\protocol\proto.c
COPY libel\protocol\proto.c sample_client_synergy\protocol\proto.c >nul

:: sample_client_synergy\protocol\proto.h
COPY libel\protocol\proto.h sample_client_synergy\protocol\proto.h >nul
