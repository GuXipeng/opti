declare { i64, i1 } @llvm.uadd.with.overflow.i64(i64, i64)

define { i64, i1 } @add_with_carry(i64 %x, i64 %y, i1 %c) alwaysinline naked
{
  %vc1 = call { i64, i1 } @llvm.uadd.with.overflow.i64(i64 %x, i64 %y)
  %v1 = extractvalue { i64, i1 } %vc1, 0
  %c1 = extractvalue { i64, i1 } %vc1, 1
  %zc = zext i1 %c to i64
  %v2 = add i64 %v1, %zc
  %r1 = insertvalue { i64, i1 } undef, i64 %v2, 0
  %r2 = insertvalue { i64, i1 } %r1, i1 %c1, 1
  ret { i64, i1 } %r2
}
define zeroext i1 @addnLLVM(i64* %pz, i64* %px, i64* %py, i64 %n) {
entry:
  %nZero = icmp eq i64 %n, 0
  br i1 %nZero, label %exit, label %lp

lp:
  %i_p = phi i64 [0, %entry], [%i, %lp]
  %c_p = phi i1 [0, %entry], [%c, %lp]

  %px_i = getelementptr i64* %px, i64 %i_p
  %py_i = getelementptr i64* %py, i64 %i_p
  %pz_i = getelementptr i64* %pz, i64 %i_p
  %x = load i64* %px_i, align 64
  %y = load i64* %py_i, align 64

;  %rc1 = call { i64, i1 } @llvm.uadd.with.overflow.i64(i64 %x, i64 %y)
;  %r1 = extractvalue { i64, i1 } %rc1, 0
;  %c = extractvalue { i64, i1 } %rc1, 1
;  %zc = zext i1 %c_p to i64
;  %r2 = add i64 %r1, %zc

  %rc1 = call { i64, i1 } @add_with_carry(i64 %x, i64 %y, i1 %c_p)
  %r2 = extractvalue { i64, i1 } %rc1, 0
  %c = extractvalue { i64, i1 } %rc1, 1

  store i64 %r2, i64* %pz_i

  %i = add i64 %i_p, 1

  %i_eq_n = icmp eq i64 %i, %n
  br i1 %i_eq_n, label %exit, label %lp

exit:
  %r = phi i1 [0, %entry], [%c, %lp]
  ret i1 %r
}

