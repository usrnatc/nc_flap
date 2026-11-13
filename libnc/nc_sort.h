#if !defined(__NC_SORT_H__)
#define __NC_SORT_H__

#include "nc_types.h"

// @defines____________________________________________________________________
#define NC_DEFINE_QUICKSORT(FuncName, Type, CmpFunc)                                                                      \
    internal void QuickSort##FuncName##Impl(Type* Arr, dptr Left, dptr Right)                                             \
    {                                                                                                                     \
        while (Left < Right)                                                                                              \
        {                                                                                                                 \
            if (Right - Left < 16)                                                                                        \
            {                                                                                                             \
                for (dptr I = Left + 1; I <= Right; ++I) {                                                                \
                    Type Key = Arr[I];                                                                                    \
                    dptr J = I - 1;                                                                                       \
                    while (J >= Left && CmpFunc(&Key, &Arr[J]) < 0)                                                       \
                    {                                                                                                     \
                        Arr[J + 1] = Arr[J];                                                                              \
                        --J;                                                                                              \
                    }                                                                                                     \
                    Arr[J + 1] = Key;                                                                                     \
                }                                                                                                         \
                break;                                                                                                    \
            }                                                                                                             \
            dptr Mid = Left + (Right - Left) / 2;                                                                         \
            if (CmpFunc(&Arr[Mid], &Arr[Left]) < 0)  { Type Tmp = Arr[Left]; Arr[Left] = Arr[Mid]; Arr[Mid] = Tmp; }      \
            if (CmpFunc(&Arr[Right], &Arr[Left]) < 0) { Type Tmp = Arr[Left]; Arr[Left] = Arr[Right]; Arr[Right] = Tmp; } \
            if (CmpFunc(&Arr[Right], &Arr[Mid]) < 0)  { Type Tmp = Arr[Mid]; Arr[Mid] = Arr[Right]; Arr[Right] = Tmp; }   \
            Type Pivot = Arr[Mid];                                                                                        \
            dptr I = Left - 1;                                                                                            \
            dptr J = Right + 1;                                                                                           \
            while (TRUE)                                                                                                  \
            {                                                                                                             \
                do { I++; } while (CmpFunc(&Arr[I], &Pivot) < 0);                                                         \
                do { J--; } while (CmpFunc(&Pivot, &Arr[J]) < 0);                                                         \
                if (I >= J) break;                                                                                        \
                Type Tmp = Arr[I];                                                                                        \
                Arr[I] = Arr[J];                                                                                          \
                Arr[J] = Tmp;                                                                                             \
            }                                                                                                             \
            if (J - Left < Right - J - 1)                                                                                 \
            {                                                                                                             \
                QuickSort##FuncName##Impl(Arr, Left, J);                                                                  \
                Left = J + 1;                                                                                             \
            }                                                                                                             \
            else                                                                                                          \
            {                                                                                                             \
                QuickSort##FuncName##Impl(Arr, J + 1, Right);                                                             \
                Right = J;                                                                                                \
            }                                                                                                             \
        }                                                                                                                 \
    }                                                                                                                     \
    internal void QuickSort##FuncName(Type* Base, size_t Count)                                                           \
    {                                                                                                                     \
        if (Count > 1)                                                                                                    \
            QuickSort##FuncName##Impl(Base, 0, (dptr) Count - 1);                                                         \
    }

// @types______________________________________________________________________


// @runtime____________________________________________________________________


// @functions__________________________________________________________________


#endif // __NC_SORT_H__
