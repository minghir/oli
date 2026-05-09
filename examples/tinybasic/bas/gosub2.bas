10 PRINT "Inceput program principal"
20 LET X = 5
30 GOSUB 100
40 PRINT "Inapoi in principal. Patratul lui 5 este:";
50 PRINT P
60 LET X = 9
70 GOSUB 100
80 PRINT "Patratul lui 9 este:";
90 PRINT P
95 END

100 REM --- SUBRUTINA: Calculeaza Patratul lui X ---
110 LET P = X * X
120 RETURN