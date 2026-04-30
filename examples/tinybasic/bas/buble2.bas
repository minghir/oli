10 REM Initializare numere in tb_vars (A-E)
20 LET A = 55
30 LET B = 12
40 LET C = 98
50 LET D = 45
60 LET E = 23
70 PRINT "Sir initial: "
80 PRINT A
90 PRINT B
100 PRINT C
110 PRINT D
120 PRINT E
200 REM Algoritm Bubble Sort (5 elemente)
210 LET F = 0
220 REM Pasul 1: Comparam A cu B
230 IF A <= B THEN GOTO 270
240 LET T = A
250 LET A = B
260 LET B = T
270 REM Pasul 2: Comparam B cu C
280 IF B <= C THEN GOTO 320
290 LET T = B
300 LET B = C
310 LET C = T
320 REM Pasul 3: Comparam C cu D
330 IF C <= D THEN GOTO 370
340 LET T = C
350 LET C = D
360 LET D = T
370 REM Pasul 4: Comparam D cu E
380 IF D <= E THEN GOTO 420
390 LET T = D
400 LET D = E
410 LET E = T
420 REM Daca am facut modificari, mai trecem o data
430 REM In Tiny BASIC clasic am repeta bucla de X ori
440 LET F = F + 1
450 IF F < 5 THEN GOTO 220
500 PRINT "Sir sortat: "
510 PRINT A
520 PRINT B
530 PRINT C
540 PRINT D
550 PRINT E
560 END
