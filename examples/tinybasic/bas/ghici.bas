10 CLS
20 PRINT "--- JOCUL NUMERELOR (Oli-BASIC) ---"
30 LET X = RND(1, 100)
40 PRINT "M-am gandit la un numar intre 1 si 100."
50 LET I = 0
60 REM --- Inceputul buclei de joc ---
70 LET I = I + 1
80 INPUT "Ghiceste: ", G
90 IF G = X THEN GOTO 140
100 IF G < X THEN PRINT "Prea mic! Mai incearca."
110 IF G > X THEN PRINT "Prea mare! Mai incearca."
120 REM --- ATENTIE: Sarim la 70 ca sa crestem contorul! ---
130 GOTO 70
140 PRINT "FELICITARI! L-ai ghicit din ";
150 PRINT I;
160 PRINT " incercari."
170 PRINT ""
180 INPUT "Joci iar? (1-DA 0-NU): ", R
190 IF R = 1 THEN GOTO 10
200 PRINT "La revedere!"
210 END
