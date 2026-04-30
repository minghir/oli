10 PRINT "1. [Main] Pornire"
20 GOSUB 100
30 PRINT "6. [Main] Inapoi in Main. Test reusit!"
40 END
100 PRINT "2. [Sub1] Intrat in Sub1"
110 GOSUB 200
120 PRINT "5. [Sub1] Inapoi din Sub2"
130 RETURN
200 PRINT "3. [Sub2] Intrat in Sub2"
210 PRINT "4. [Sub2] Ne intoarcem din Sub2"
220 RETURN
