; ModuleID = 'TestConstSetter.cpp'
source_filename = "TestConstSetter.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%class.ConstSetter = type { i32 }

$_ZN11ConstSetterC2Ei = comdat any

$_ZNK11ConstSetter6setValEi = comdat any

; Function Attrs: nounwind uwtable
define linkonce_odr dso_local void @_ZN11ConstSetterC2Ei(%class.ConstSetter* %this, i32 %val) unnamed_addr #0 comdat align 2 !dbg !7 {
entry:
  %this.addr = alloca %class.ConstSetter*, align 8
  %val.addr = alloca i32, align 4
  store %class.ConstSetter* %this, %class.ConstSetter** %this.addr, align 8, !tbaa !18
  call void @llvm.dbg.declare(metadata %class.ConstSetter** %this.addr, metadata !15, metadata !DIExpression()), !dbg !22
  store i32 %val, i32* %val.addr, align 4, !tbaa !23
  call void @llvm.dbg.declare(metadata i32* %val.addr, metadata !17, metadata !DIExpression()), !dbg !25
  %this1 = load %class.ConstSetter*, %class.ConstSetter** %this.addr, align 8
  %0 = load i32, i32* %val.addr, align 4, !dbg !26, !tbaa !23
  %value = getelementptr inbounds %class.ConstSetter, %class.ConstSetter* %this1, i32 0, i32 0, !dbg !28
  store i32 %0, i32* %value, align 4, !dbg !29, !tbaa !30
  ret void, !dbg !32
}

; Function Attrs: nounwind readnone speculatable
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind uwtable
define linkonce_odr dso_local void @_ZNK11ConstSetter6setValEi(%class.ConstSetter* %this, i32 %new_val) #0 comdat align 2 !dbg !33 {
entry:
  %this.addr = alloca %class.ConstSetter*, align 8
  %new_val.addr = alloca i32, align 4
  store %class.ConstSetter* %this, %class.ConstSetter** %this.addr, align 8, !tbaa !18
  call void @llvm.dbg.declare(metadata %class.ConstSetter** %this.addr, metadata !40, metadata !DIExpression()), !dbg !43
  store i32 %new_val, i32* %new_val.addr, align 4, !tbaa !23
  call void @llvm.dbg.declare(metadata i32* %new_val.addr, metadata !42, metadata !DIExpression()), !dbg !44
  %this1 = load %class.ConstSetter*, %class.ConstSetter** %this.addr, align 8
  %0 = load i32, i32* %new_val.addr, align 4, !dbg !45, !tbaa !23
  %value = getelementptr inbounds %class.ConstSetter, %class.ConstSetter* %this1, i32 0, i32 0, !dbg !46
  store i32 %0, i32* %value, align 4, !dbg !47, !tbaa !30
  ret void, !dbg !48
}

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus, file: !1, producer: "clang version 8.0.1 (tags/RELEASE_801/final)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, nameTableKind: None)
!1 = !DIFile(filename: "TestConstSetter.cpp", directory: "/home/vagrant/hello-world-pass/tests")
!2 = !{}
!3 = !{i32 2, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 8.0.1 (tags/RELEASE_801/final)"}
!7 = distinct !DISubprogram(name: "ConstSetter", linkageName: "_ZN11ConstSetterC2Ei", scope: !8, file: !1, line: 5, type: !9, scopeLine: 5, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, declaration: !13, retainedNodes: !14)
!8 = !DICompositeType(tag: DW_TAG_class_type, name: "ConstSetter", file: !1, line: 1, flags: DIFlagFwdDecl, identifier: "_ZTS11ConstSetter")
!9 = !DISubroutineType(types: !10)
!10 = !{null, !11, !12}
!11 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !8, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!12 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!13 = !DISubprogram(name: "ConstSetter", scope: !8, file: !1, line: 5, type: !9, scopeLine: 5, flags: DIFlagPublic | DIFlagPrototyped, spFlags: DISPFlagOptimized)
!14 = !{!15, !17}
!15 = !DILocalVariable(name: "this", arg: 1, scope: !7, type: !16, flags: DIFlagArtificial | DIFlagObjectPointer)
!16 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !8, size: 64)
!17 = !DILocalVariable(name: "val", arg: 2, scope: !7, file: !1, line: 5, type: !12)
!18 = !{!19, !19, i64 0}
!19 = !{!"any pointer", !20, i64 0}
!20 = !{!"omnipotent char", !21, i64 0}
!21 = !{!"Simple C++ TBAA"}
!22 = !DILocation(line: 0, scope: !7)
!23 = !{!24, !24, i64 0}
!24 = !{!"int", !20, i64 0}
!25 = !DILocation(line: 5, column: 20, scope: !7)
!26 = !DILocation(line: 5, column: 35, scope: !27)
!27 = distinct !DILexicalBlock(scope: !7, file: !1, line: 5, column: 25)
!28 = !DILocation(line: 5, column: 27, scope: !27)
!29 = !DILocation(line: 5, column: 33, scope: !27)
!30 = !{!31, !24, i64 0}
!31 = !{!"_ZTS11ConstSetter", !24, i64 0}
!32 = !DILocation(line: 5, column: 40, scope: !7)
!33 = distinct !DISubprogram(name: "setVal", linkageName: "_ZNK11ConstSetter6setValEi", scope: !8, file: !1, line: 6, type: !34, scopeLine: 6, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, declaration: !38, retainedNodes: !39)
!34 = !DISubroutineType(types: !35)
!35 = !{null, !36, !12}
!36 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !37, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!37 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !8)
!38 = !DISubprogram(name: "setVal", linkageName: "_ZNK11ConstSetter6setValEi", scope: !8, file: !1, line: 6, type: !34, scopeLine: 6, flags: DIFlagPublic | DIFlagPrototyped, spFlags: DISPFlagOptimized)
!39 = !{!40, !42}
!40 = !DILocalVariable(name: "this", arg: 1, scope: !33, type: !41, flags: DIFlagArtificial | DIFlagObjectPointer)
!41 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !37, size: 64)
!42 = !DILocalVariable(name: "new_val", arg: 2, scope: !33, file: !1, line: 6, type: !12)
!43 = !DILocation(line: 0, scope: !33)
!44 = !DILocation(line: 6, column: 20, scope: !33)
!45 = !DILocation(line: 6, column: 45, scope: !33)
!46 = !DILocation(line: 6, column: 37, scope: !33)
!47 = !DILocation(line: 6, column: 43, scope: !33)
!48 = !DILocation(line: 6, column: 54, scope: !33)
