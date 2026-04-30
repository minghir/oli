10 CLS
20 PRINT "Mandelbrot in Oli-BASIC (Comparison Test)"
30 LET M = 15
40 REM --- Intervalul Y ---
50 LET Y = -1.2
60 REM --- Intervalul X ---
70 LET X = -2.0
80 LET A = X
90 LET B = Y
100 LET U = 0
110 LET V = 0
120 LET I = 0
130 REM --- Bucla de Iteratie ---
140 LET R = U * U - V * V + A
150 LET S = 2.0 * U * V + B
160 LET U = R
170 LET V = S
180 LET I = I + 1
190 IF I == M THEN GOTO 220
200 IF (U * U + V * V) > 4.0 THEN GOTO 220
210 GOTO 140
220 REM --- Afisare caracter ---
230 IF I == M THEN PRINT "*";
240 IF I < M THEN PRINT ".";
250 REM --- Incrementare X ---
260 LET X = X + 0.05
270 IF X < 0.5 THEN GOTO 80
280 PRINT ""
290 REM --- Incrementare Y ---
300 LET Y = Y + 0.1
310 IF Y < 1.2 THEN GOTO 60
320 PRINT "Gata."
