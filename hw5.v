module hw5_4 (a, b, c, d, e, y2, y3);
    input a, b, c, d, e, y2, y3;
    output out;
    wire w1, w2, w3, w4, w5;

    xor(w1, a, b, c);
    xor(w2, b, c, d, e);
    xor(w3, a, d, e);
    xnor(w4, y2, w1);
    xnor(w5, y3, w2);
    and(out, w3, w4, w5);
   
endmodule