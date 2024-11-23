== Exercise 4.4

#let det = math.mat.with(delim: "|")

$
alpha &= det(
  24, -13, 7, 9, 5;
  11, 16, -37, 99, 64;
  1, 4, 2, 2, -3;
  31, -42, 78, 55, -3;
  62, 47, 29, -14, -8;
)
$

The expansion of $beta$ results in exactly the same $4 times 4$
matrices as in the expansion of $alpha$. The difference is that
each coefficient is three times the corresponding coefficient in
the $alpha$ expansion. Thus we can factor out this 3 to arrive
at $beta = 3 alpha$.

This demonstrates Theorem 4.3 from the text, the
_Row Scalar Property_.
