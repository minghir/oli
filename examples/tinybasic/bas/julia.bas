10 CLS
20 PRINT "Julia Set in Oli-BASIC (C = -0.7, 0.27)"
30 LET M = 15
40 REM --- Constantele Juliei (Schimba-le pentru forme noi!) ---
50 LET CA = -0.7
60 LET CB = 0.27
70 REM --- Intervalul Y ---
80 LET Y = -1.2
90 REM --- Intervalul X ---
100 LET X = -1.5
110 LET U = X
120 LET V = Y
130 LET I = 0
140 REM --- Bucla de Iteratie (z = z^2 + C_fix) ---
150 LET R = U * U - V * V + CA
160 LET S = 2.0 * U * V + CB
170 LET U = R
180 LET V = S
190 LET I = I + 1
200 IF I = M THEN GOTO 230
210 IF (U * U + V * V) > 4.0 THEN GOTO 230
220 GOTO 140
230 REM --- Afisare caracter ---
240 IF I = M THEN PRINT "*";
250 IF I < M THEN PRINT ".";
260 REM --- Incrementare X ---
270 LET X = X + 0.05
280 IF X < 1.5 THEN GOTO 110
290 PRINT ""
300 REM --- Incrementare Y ---
310 LET Y = Y + 0.1
320 IF Y < 1.2 THEN GOTO 100
330 PRINT "Explorare Julia terminata."