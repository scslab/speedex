# Original form

For each asset pair (A, B), the variable y_{AB} denotes the amount of money sent from A to B.

## LP

max \Sigma_{A,B} y_{AB}

subject to:

L_{AB} \leq y_{AB} \leq U_{AB}

\Sigma_B y_{AB} = \Sigma_B y_{BA}

# Rearranging the LP

The first constraint becomes

y_{AB} + e_{AB} = (U_{AB} - L_{AB})

for e_{AB} a new slack variable (meaning of y_{AB} is nominally changed, but same obj fn).

\Sigma_B (y_{AB} + L_{AB}) >= (note the change, >= not =) \Sigma_B (y_{BA} + L_{BA}})

(amount of money selling A is at least as much buying A, or in other words, supply(A) > demand(A))

Or, with a new slack variable, this becomes

\Sigma_B (y_{AB} + L_{AB}) = \Sigma_B (y_{BA} + L_{BA}}) + s_A


This last slack variable isn't strictly necessary to put the LP into standard form, but it ensures that 
the constraints of the LP are always linearly independent.

Easy to see this constraint matrix is totally unimodular (e.g. the proof where you assign +/-1 to sets of rows, see wikipedia characterizations of total unimodularity)

# Feasibility Check

This LP is always feasible if L_{AB} = 0.  Other values of L_{AB} might not be.

We introduce therefore a second set of slack variables, t_A to get the constraint

\Sigma_B (y_{AB} + L_{AB}) + t_A = \Sigma_B (y_{BA} + L_{BA}}) + s_A

Clearly, this LP is always feasible with appropriately chosen values of t_A and s_A.

t_A represents the amount that something is overdemanded.  The original LP is feasible if all t_As are 0.

Hence, we can solve the optimization problem:

## Feasibility LP

min \Sigma t_A

st

y_{AB} + e_{AB} = (U_{AB} - L_{AB})

\Sigma_B (y_{AB} + L_{AB}) + t_A = \Sigma_B (y_{BA} + L_{BA}}) + s_A

(or equiv, demand - supply = 0)

\Sigma_B y_{BA} + s_A - \Sigma_B y_{AB} - t_A = -\Sigma_B L_{BA} + \Sigma_B L_{AB}

for (sell A, buy B) pair, neg on buy, pos on sell (neg on swap, pos on regular)

y_{AB}, e_{AB}, t_A, s_A all >=0

## Impl

When checking feasibility as a Tatonnement shortcut, we do not need to recover a solution to this LP.  However, if necessary, we could, using the same techniques as in the regular LP.  

We simply need to check whether the objective value is 0.

## Note

We can't collapse the t_As into one set of constraints, as that would violate total unimodularity.



