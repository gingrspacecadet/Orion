.section .text
.global enable_ints
enable_ints:
    flagsw #0x3
    ret

; TODO: no need to save when coming from user mode
__int_common:
    ; save the active kernel register bank.
    push {r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14}

    ; ensure sp is 8-byte aligned before call.
    add r1, rsp, #0
    and r1, r1, #4
    jeq r1, #0, $__int_call_aligned

    sub rsp, rsp, #4
    call $int_handler
    add rsp, rsp, #4
    jmp $__int_call_done

__int_call_aligned:
    call $int_handler

__int_call_done:
    ; restore the interrupted kernel register bank.
    pop {r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14}
    pop r0
    iret

__int_handler_0:
    push r0
    mov r0, #0
    jmp $__int_common

__int_handler_1:
    push r0
    mov r0, #1
    jmp $__int_common

__int_handler_2:
    push r0
    mov r0, #2
    jmp $__int_common

__int_handler_3:
    push r0
    mov r0, #3
    jmp $__int_common

__int_handler_4:
    push r0
    mov r0, #4
    jmp $__int_common

__int_handler_5:
    push r0
    mov r0, #5
    jmp $__int_common

__int_handler_6:
    push r0
    mov r0, #6
    jmp $__int_common

__int_handler_7:
    push r0
    mov r0, #7
    jmp $__int_common

__int_handler_8:
    push r0
    mov r0, #8
    jmp $__int_common

__int_handler_9:
    push r0
    mov r0, #9
    jmp $__int_common

__int_handler_10:
    push r0
    mov r0, #10
    jmp $__int_common

__int_handler_11:
    push r0
    mov r0, #11
    jmp $__int_common

__int_handler_12:
    push r0
    mov r0, #12
    jmp $__int_common

__int_handler_13:
    push r0
    mov r0, #13
    jmp $__int_common

__int_handler_14:
    push r0
    mov r0, #14
    jmp $__int_common

__int_handler_15:
    push r0
    mov r0, #15
    jmp $__int_common

__int_handler_16:
    push r0
    mov r0, #16
    jmp $__int_common

__int_handler_17:
    push r0
    mov r0, #17
    jmp $__int_common

__int_handler_18:
    push r0
    mov r0, #18
    jmp $__int_common

__int_handler_19:
    push r0
    mov r0, #19
    jmp $__int_common

__int_handler_20:
    push r0
    mov r0, #20
    jmp $__int_common

__int_handler_21:
    push r0
    mov r0, #21
    jmp $__int_common

__int_handler_22:
    push r0
    mov r0, #22
    jmp $__int_common

__int_handler_23:
    push r0
    mov r0, #23
    jmp $__int_common

__int_handler_24:
    push r0
    mov r0, #24
    jmp $__int_common

__int_handler_25:
    push r0
    mov r0, #25
    jmp $__int_common

__int_handler_26:
    push r0
    mov r0, #26
    jmp $__int_common

__int_handler_27:
    push r0
    mov r0, #27
    jmp $__int_common

__int_handler_28:
    push r0
    mov r0, #28
    jmp $__int_common

__int_handler_29:
    push r0
    mov r0, #29
    jmp $__int_common

__int_handler_30:
    push r0
    mov r0, #30
    jmp $__int_common

__int_handler_31:
    push r0
    mov r0, #31
    jmp $__int_common

__int_handler_32:
    push r0
    mov r0, #32
    jmp $__int_common

__int_handler_33:
    push r0
    mov r0, #33
    jmp $__int_common

__int_handler_34:
    push r0
    mov r0, #34
    jmp $__int_common

__int_handler_35:
    push r0
    mov r0, #35
    jmp $__int_common

__int_handler_36:
    push r0
    mov r0, #36
    jmp $__int_common

__int_handler_37:
    push r0
    mov r0, #37
    jmp $__int_common

__int_handler_38:
    push r0
    mov r0, #38
    jmp $__int_common

__int_handler_39:
    push r0
    mov r0, #39
    jmp $__int_common

__int_handler_40:
    push r0
    mov r0, #40
    jmp $__int_common

__int_handler_41:
    push r0
    mov r0, #41
    jmp $__int_common

__int_handler_42:
    push r0
    mov r0, #42
    jmp $__int_common

__int_handler_43:
    push r0
    mov r0, #43
    jmp $__int_common

__int_handler_44:
    push r0
    mov r0, #44
    jmp $__int_common

__int_handler_45:
    push r0
    mov r0, #45
    jmp $__int_common

__int_handler_46:
    push r0
    mov r0, #46
    jmp $__int_common

__int_handler_47:
    push r0
    mov r0, #47
    jmp $__int_common

__int_handler_48:
    push r0
    mov r0, #48
    jmp $__int_common

__int_handler_49:
    push r0
    mov r0, #49
    jmp $__int_common

__int_handler_50:
    push r0
    mov r0, #50
    jmp $__int_common

__int_handler_51:
    push r0
    mov r0, #51
    jmp $__int_common

__int_handler_52:
    push r0
    mov r0, #52
    jmp $__int_common

__int_handler_53:
    push r0
    mov r0, #53
    jmp $__int_common

__int_handler_54:
    push r0
    mov r0, #54
    jmp $__int_common

__int_handler_55:
    push r0
    mov r0, #55
    jmp $__int_common

__int_handler_56:
    push r0
    mov r0, #56
    jmp $__int_common

__int_handler_57:
    push r0
    mov r0, #57
    jmp $__int_common

__int_handler_58:
    push r0
    mov r0, #58
    jmp $__int_common

__int_handler_59:
    push r0
    mov r0, #59
    jmp $__int_common

__int_handler_60:
    push r0
    mov r0, #60
    jmp $__int_common

__int_handler_61:
    push r0
    mov r0, #61
    jmp $__int_common

__int_handler_62:
    push r0
    mov r0, #62
    jmp $__int_common

__int_handler_63:
    push r0
    mov r0, #63
    jmp $__int_common

__int_handler_64:
    push r0
    mov r0, #64
    jmp $__int_common

__int_handler_65:
    push r0
    mov r0, #65
    jmp $__int_common

__int_handler_66:
    push r0
    mov r0, #66
    jmp $__int_common

__int_handler_67:
    push r0
    mov r0, #67
    jmp $__int_common

__int_handler_68:
    push r0
    mov r0, #68
    jmp $__int_common

__int_handler_69:
    push r0
    mov r0, #69
    jmp $__int_common

__int_handler_70:
    push r0
    mov r0, #70
    jmp $__int_common

__int_handler_71:
    push r0
    mov r0, #71
    jmp $__int_common

__int_handler_72:
    push r0
    mov r0, #72
    jmp $__int_common

__int_handler_73:
    push r0
    mov r0, #73
    jmp $__int_common

__int_handler_74:
    push r0
    mov r0, #74
    jmp $__int_common

__int_handler_75:
    push r0
    mov r0, #75
    jmp $__int_common

__int_handler_76:
    push r0
    mov r0, #76
    jmp $__int_common

__int_handler_77:
    push r0
    mov r0, #77
    jmp $__int_common

__int_handler_78:
    push r0
    mov r0, #78
    jmp $__int_common

__int_handler_79:
    push r0
    mov r0, #79
    jmp $__int_common

__int_handler_80:
    push r0
    mov r0, #80
    jmp $__int_common

__int_handler_81:
    push r0
    mov r0, #81
    jmp $__int_common

__int_handler_82:
    push r0
    mov r0, #82
    jmp $__int_common

__int_handler_83:
    push r0
    mov r0, #83
    jmp $__int_common

__int_handler_84:
    push r0
    mov r0, #84
    jmp $__int_common

__int_handler_85:
    push r0
    mov r0, #85
    jmp $__int_common

__int_handler_86:
    push r0
    mov r0, #86
    jmp $__int_common

__int_handler_87:
    push r0
    mov r0, #87
    jmp $__int_common

__int_handler_88:
    push r0
    mov r0, #88
    jmp $__int_common

__int_handler_89:
    push r0
    mov r0, #89
    jmp $__int_common

__int_handler_90:
    push r0
    mov r0, #90
    jmp $__int_common

__int_handler_91:
    push r0
    mov r0, #91
    jmp $__int_common

__int_handler_92:
    push r0
    mov r0, #92
    jmp $__int_common

__int_handler_93:
    push r0
    mov r0, #93
    jmp $__int_common

__int_handler_94:
    push r0
    mov r0, #94
    jmp $__int_common

__int_handler_95:
    push r0
    mov r0, #95
    jmp $__int_common

__int_handler_96:
    push r0
    mov r0, #96
    jmp $__int_common

__int_handler_97:
    push r0
    mov r0, #97
    jmp $__int_common

__int_handler_98:
    push r0
    mov r0, #98
    jmp $__int_common

__int_handler_99:
    push r0
    mov r0, #99
    jmp $__int_common

__int_handler_100:
    push r0
    mov r0, #100
    jmp $__int_common

__int_handler_101:
    push r0
    mov r0, #101
    jmp $__int_common

__int_handler_102:
    push r0
    mov r0, #102
    jmp $__int_common

__int_handler_103:
    push r0
    mov r0, #103
    jmp $__int_common

__int_handler_104:
    push r0
    mov r0, #104
    jmp $__int_common

__int_handler_105:
    push r0
    mov r0, #105
    jmp $__int_common

__int_handler_106:
    push r0
    mov r0, #106
    jmp $__int_common

__int_handler_107:
    push r0
    mov r0, #107
    jmp $__int_common

__int_handler_108:
    push r0
    mov r0, #108
    jmp $__int_common

__int_handler_109:
    push r0
    mov r0, #109
    jmp $__int_common

__int_handler_110:
    push r0
    mov r0, #110
    jmp $__int_common

__int_handler_111:
    push r0
    mov r0, #111
    jmp $__int_common

__int_handler_112:
    push r0
    mov r0, #112
    jmp $__int_common

__int_handler_113:
    push r0
    mov r0, #113
    jmp $__int_common

__int_handler_114:
    push r0
    mov r0, #114
    jmp $__int_common

__int_handler_115:
    push r0
    mov r0, #115
    jmp $__int_common

__int_handler_116:
    push r0
    mov r0, #116
    jmp $__int_common

__int_handler_117:
    push r0
    mov r0, #117
    jmp $__int_common

__int_handler_118:
    push r0
    mov r0, #118
    jmp $__int_common

__int_handler_119:
    push r0
    mov r0, #119
    jmp $__int_common

__int_handler_120:
    push r0
    mov r0, #120
    jmp $__int_common

__int_handler_121:
    push r0
    mov r0, #121
    jmp $__int_common

__int_handler_122:
    push r0
    mov r0, #122
    jmp $__int_common

__int_handler_123:
    push r0
    mov r0, #123
    jmp $__int_common

__int_handler_124:
    push r0
    mov r0, #124
    jmp $__int_common

__int_handler_125:
    push r0
    mov r0, #125
    jmp $__int_common

__int_handler_126:
    push r0
    mov r0, #126
    jmp $__int_common

__int_handler_127:
    push r0
    mov r0, #127
    jmp $__int_common

__int_handler_128:
    push r0
    mov r0, #128
    jmp $__int_common

__int_handler_129:
    push r0
    mov r0, #129
    jmp $__int_common

__int_handler_130:
    push r0
    mov r0, #130
    jmp $__int_common

__int_handler_131:
    push r0
    mov r0, #131
    jmp $__int_common

__int_handler_132:
    push r0
    mov r0, #132
    jmp $__int_common

__int_handler_133:
    push r0
    mov r0, #133
    jmp $__int_common

__int_handler_134:
    push r0
    mov r0, #134
    jmp $__int_common

__int_handler_135:
    push r0
    mov r0, #135
    jmp $__int_common

__int_handler_136:
    push r0
    mov r0, #136
    jmp $__int_common

__int_handler_137:
    push r0
    mov r0, #137
    jmp $__int_common

__int_handler_138:
    push r0
    mov r0, #138
    jmp $__int_common

__int_handler_139:
    push r0
    mov r0, #139
    jmp $__int_common

__int_handler_140:
    push r0
    mov r0, #140
    jmp $__int_common

__int_handler_141:
    push r0
    mov r0, #141
    jmp $__int_common

__int_handler_142:
    push r0
    mov r0, #142
    jmp $__int_common

__int_handler_143:
    push r0
    mov r0, #143
    jmp $__int_common

__int_handler_144:
    push r0
    mov r0, #144
    jmp $__int_common

__int_handler_145:
    push r0
    mov r0, #145
    jmp $__int_common

__int_handler_146:
    push r0
    mov r0, #146
    jmp $__int_common

__int_handler_147:
    push r0
    mov r0, #147
    jmp $__int_common

__int_handler_148:
    push r0
    mov r0, #148
    jmp $__int_common

__int_handler_149:
    push r0
    mov r0, #149
    jmp $__int_common

__int_handler_150:
    push r0
    mov r0, #150
    jmp $__int_common

__int_handler_151:
    push r0
    mov r0, #151
    jmp $__int_common

__int_handler_152:
    push r0
    mov r0, #152
    jmp $__int_common

__int_handler_153:
    push r0
    mov r0, #153
    jmp $__int_common

__int_handler_154:
    push r0
    mov r0, #154
    jmp $__int_common

__int_handler_155:
    push r0
    mov r0, #155
    jmp $__int_common

__int_handler_156:
    push r0
    mov r0, #156
    jmp $__int_common

__int_handler_157:
    push r0
    mov r0, #157
    jmp $__int_common

__int_handler_158:
    push r0
    mov r0, #158
    jmp $__int_common

__int_handler_159:
    push r0
    mov r0, #159
    jmp $__int_common

__int_handler_160:
    push r0
    mov r0, #160
    jmp $__int_common

__int_handler_161:
    push r0
    mov r0, #161
    jmp $__int_common

__int_handler_162:
    push r0
    mov r0, #162
    jmp $__int_common

__int_handler_163:
    push r0
    mov r0, #163
    jmp $__int_common

__int_handler_164:
    push r0
    mov r0, #164
    jmp $__int_common

__int_handler_165:
    push r0
    mov r0, #165
    jmp $__int_common

__int_handler_166:
    push r0
    mov r0, #166
    jmp $__int_common

__int_handler_167:
    push r0
    mov r0, #167
    jmp $__int_common

__int_handler_168:
    push r0
    mov r0, #168
    jmp $__int_common

__int_handler_169:
    push r0
    mov r0, #169
    jmp $__int_common

__int_handler_170:
    push r0
    mov r0, #170
    jmp $__int_common

__int_handler_171:
    push r0
    mov r0, #171
    jmp $__int_common

__int_handler_172:
    push r0
    mov r0, #172
    jmp $__int_common

__int_handler_173:
    push r0
    mov r0, #173
    jmp $__int_common

__int_handler_174:
    push r0
    mov r0, #174
    jmp $__int_common

__int_handler_175:
    push r0
    mov r0, #175
    jmp $__int_common

__int_handler_176:
    push r0
    mov r0, #176
    jmp $__int_common

__int_handler_177:
    push r0
    mov r0, #177
    jmp $__int_common

__int_handler_178:
    push r0
    mov r0, #178
    jmp $__int_common

__int_handler_179:
    push r0
    mov r0, #179
    jmp $__int_common

__int_handler_180:
    push r0
    mov r0, #180
    jmp $__int_common

__int_handler_181:
    push r0
    mov r0, #181
    jmp $__int_common

__int_handler_182:
    push r0
    mov r0, #182
    jmp $__int_common

__int_handler_183:
    push r0
    mov r0, #183
    jmp $__int_common

__int_handler_184:
    push r0
    mov r0, #184
    jmp $__int_common

__int_handler_185:
    push r0
    mov r0, #185
    jmp $__int_common

__int_handler_186:
    push r0
    mov r0, #186
    jmp $__int_common

__int_handler_187:
    push r0
    mov r0, #187
    jmp $__int_common

__int_handler_188:
    push r0
    mov r0, #188
    jmp $__int_common

__int_handler_189:
    push r0
    mov r0, #189
    jmp $__int_common

__int_handler_190:
    push r0
    mov r0, #190
    jmp $__int_common

__int_handler_191:
    push r0
    mov r0, #191
    jmp $__int_common

__int_handler_192:
    push r0
    mov r0, #192
    jmp $__int_common

__int_handler_193:
    push r0
    mov r0, #193
    jmp $__int_common

__int_handler_194:
    push r0
    mov r0, #194
    jmp $__int_common

__int_handler_195:
    push r0
    mov r0, #195
    jmp $__int_common

__int_handler_196:
    push r0
    mov r0, #196
    jmp $__int_common

__int_handler_197:
    push r0
    mov r0, #197
    jmp $__int_common

__int_handler_198:
    push r0
    mov r0, #198
    jmp $__int_common

__int_handler_199:
    push r0
    mov r0, #199
    jmp $__int_common

__int_handler_200:
    push r0
    mov r0, #200
    jmp $__int_common

__int_handler_201:
    push r0
    mov r0, #201
    jmp $__int_common

__int_handler_202:
    push r0
    mov r0, #202
    jmp $__int_common

__int_handler_203:
    push r0
    mov r0, #203
    jmp $__int_common

__int_handler_204:
    push r0
    mov r0, #204
    jmp $__int_common

__int_handler_205:
    push r0
    mov r0, #205
    jmp $__int_common

__int_handler_206:
    push r0
    mov r0, #206
    jmp $__int_common

__int_handler_207:
    push r0
    mov r0, #207
    jmp $__int_common

__int_handler_208:
    push r0
    mov r0, #208
    jmp $__int_common

__int_handler_209:
    push r0
    mov r0, #209
    jmp $__int_common

__int_handler_210:
    push r0
    mov r0, #210
    jmp $__int_common

__int_handler_211:
    push r0
    mov r0, #211
    jmp $__int_common

__int_handler_212:
    push r0
    mov r0, #212
    jmp $__int_common

__int_handler_213:
    push r0
    mov r0, #213
    jmp $__int_common

__int_handler_214:
    push r0
    mov r0, #214
    jmp $__int_common

__int_handler_215:
    push r0
    mov r0, #215
    jmp $__int_common

__int_handler_216:
    push r0
    mov r0, #216
    jmp $__int_common

__int_handler_217:
    push r0
    mov r0, #217
    jmp $__int_common

__int_handler_218:
    push r0
    mov r0, #218
    jmp $__int_common

__int_handler_219:
    push r0
    mov r0, #219
    jmp $__int_common

__int_handler_220:
    push r0
    mov r0, #220
    jmp $__int_common

__int_handler_221:
    push r0
    mov r0, #221
    jmp $__int_common

__int_handler_222:
    push r0
    mov r0, #222
    jmp $__int_common

__int_handler_223:
    push r0
    mov r0, #223
    jmp $__int_common

__int_handler_224:
    push r0
    mov r0, #224
    jmp $__int_common

__int_handler_225:
    push r0
    mov r0, #225
    jmp $__int_common

__int_handler_226:
    push r0
    mov r0, #226
    jmp $__int_common

__int_handler_227:
    push r0
    mov r0, #227
    jmp $__int_common

__int_handler_228:
    push r0
    mov r0, #228
    jmp $__int_common

__int_handler_229:
    push r0
    mov r0, #229
    jmp $__int_common

__int_handler_230:
    push r0
    mov r0, #230
    jmp $__int_common

__int_handler_231:
    push r0
    mov r0, #231
    jmp $__int_common

__int_handler_232:
    push r0
    mov r0, #232
    jmp $__int_common

__int_handler_233:
    push r0
    mov r0, #233
    jmp $__int_common

__int_handler_234:
    push r0
    mov r0, #234
    jmp $__int_common

__int_handler_235:
    push r0
    mov r0, #235
    jmp $__int_common

__int_handler_236:
    push r0
    mov r0, #236
    jmp $__int_common

__int_handler_237:
    push r0
    mov r0, #237
    jmp $__int_common

__int_handler_238:
    push r0
    mov r0, #238
    jmp $__int_common

__int_handler_239:
    push r0
    mov r0, #239
    jmp $__int_common

__int_handler_240:
    push r0
    mov r0, #240
    jmp $__int_common

__int_handler_241:
    push r0
    mov r0, #241
    jmp $__int_common

__int_handler_242:
    push r0
    mov r0, #242
    jmp $__int_common

__int_handler_243:
    push r0
    mov r0, #243
    jmp $__int_common

__int_handler_244:
    push r0
    mov r0, #244
    jmp $__int_common

__int_handler_245:
    push r0
    mov r0, #245
    jmp $__int_common

__int_handler_246:
    push r0
    mov r0, #246
    jmp $__int_common

__int_handler_247:
    push r0
    mov r0, #247
    jmp $__int_common

__int_handler_248:
    push r0
    mov r0, #248
    jmp $__int_common

__int_handler_249:
    push r0
    mov r0, #249
    jmp $__int_common

__int_handler_250:
    push r0
    mov r0, #250
    jmp $__int_common

__int_handler_251:
    push r0
    mov r0, #251
    jmp $__int_common

__int_handler_252:
    push r0
    mov r0, #252
    jmp $__int_common

__int_handler_253:
    push r0
    mov r0, #253
    jmp $__int_common

__int_handler_254:
    push r0
    mov r0, #254
    jmp $__int_common

__int_handler_255:
    push r0
    mov r0, #255
    jmp $__int_common

.section .ihvt
.word $__int_handler_0
.word $__int_handler_1
.word $__int_handler_2
.word $__int_handler_3
.word $__int_handler_4
.word $__int_handler_5
.word $__int_handler_6
.word $__int_handler_7
.word $__int_handler_8
.word $__int_handler_9
.word $__int_handler_10
.word $__int_handler_11
.word $__int_handler_12
.word $__int_handler_13
.word $__int_handler_14
.word $__int_handler_15
.word $__int_handler_16
.word $__int_handler_17
.word $__int_handler_18
.word $__int_handler_19
.word $__int_handler_20
.word $__int_handler_21
.word $__int_handler_22
.word $__int_handler_23
.word $__int_handler_24
.word $__int_handler_25
.word $__int_handler_26
.word $__int_handler_27
.word $__int_handler_28
.word $__int_handler_29
.word $__int_handler_30
.word $__int_handler_31
.word $__int_handler_32
.word $__int_handler_33
.word $__int_handler_34
.word $__int_handler_35
.word $__int_handler_36
.word $__int_handler_37
.word $__int_handler_38
.word $__int_handler_39
.word $__int_handler_40
.word $__int_handler_41
.word $__int_handler_42
.word $__int_handler_43
.word $__int_handler_44
.word $__int_handler_45
.word $__int_handler_46
.word $__int_handler_47
.word $__int_handler_48
.word $__int_handler_49
.word $__int_handler_50
.word $__int_handler_51
.word $__int_handler_52
.word $__int_handler_53
.word $__int_handler_54
.word $__int_handler_55
.word $__int_handler_56
.word $__int_handler_57
.word $__int_handler_58
.word $__int_handler_59
.word $__int_handler_60
.word $__int_handler_61
.word $__int_handler_62
.word $__int_handler_63
.word $__int_handler_64
.word $__int_handler_65
.word $__int_handler_66
.word $__int_handler_67
.word $__int_handler_68
.word $__int_handler_69
.word $__int_handler_70
.word $__int_handler_71
.word $__int_handler_72
.word $__int_handler_73
.word $__int_handler_74
.word $__int_handler_75
.word $__int_handler_76
.word $__int_handler_77
.word $__int_handler_78
.word $__int_handler_79
.word $__int_handler_80
.word $__int_handler_81
.word $__int_handler_82
.word $__int_handler_83
.word $__int_handler_84
.word $__int_handler_85
.word $__int_handler_86
.word $__int_handler_87
.word $__int_handler_88
.word $__int_handler_89
.word $__int_handler_90
.word $__int_handler_91
.word $__int_handler_92
.word $__int_handler_93
.word $__int_handler_94
.word $__int_handler_95
.word $__int_handler_96
.word $__int_handler_97
.word $__int_handler_98
.word $__int_handler_99
.word $__int_handler_100
.word $__int_handler_101
.word $__int_handler_102
.word $__int_handler_103
.word $__int_handler_104
.word $__int_handler_105
.word $__int_handler_106
.word $__int_handler_107
.word $__int_handler_108
.word $__int_handler_109
.word $__int_handler_110
.word $__int_handler_111
.word $__int_handler_112
.word $__int_handler_113
.word $__int_handler_114
.word $__int_handler_115
.word $__int_handler_116
.word $__int_handler_117
.word $__int_handler_118
.word $__int_handler_119
.word $__int_handler_120
.word $__int_handler_121
.word $__int_handler_122
.word $__int_handler_123
.word $__int_handler_124
.word $__int_handler_125
.word $__int_handler_126
.word $__int_handler_127
.word $__int_handler_128
.word $__int_handler_129
.word $__int_handler_130
.word $__int_handler_131
.word $__int_handler_132
.word $__int_handler_133
.word $__int_handler_134
.word $__int_handler_135
.word $__int_handler_136
.word $__int_handler_137
.word $__int_handler_138
.word $__int_handler_139
.word $__int_handler_140
.word $__int_handler_141
.word $__int_handler_142
.word $__int_handler_143
.word $__int_handler_144
.word $__int_handler_145
.word $__int_handler_146
.word $__int_handler_147
.word $__int_handler_148
.word $__int_handler_149
.word $__int_handler_150
.word $__int_handler_151
.word $__int_handler_152
.word $__int_handler_153
.word $__int_handler_154
.word $__int_handler_155
.word $__int_handler_156
.word $__int_handler_157
.word $__int_handler_158
.word $__int_handler_159
.word $__int_handler_160
.word $__int_handler_161
.word $__int_handler_162
.word $__int_handler_163
.word $__int_handler_164
.word $__int_handler_165
.word $__int_handler_166
.word $__int_handler_167
.word $__int_handler_168
.word $__int_handler_169
.word $__int_handler_170
.word $__int_handler_171
.word $__int_handler_172
.word $__int_handler_173
.word $__int_handler_174
.word $__int_handler_175
.word $__int_handler_176
.word $__int_handler_177
.word $__int_handler_178
.word $__int_handler_179
.word $__int_handler_180
.word $__int_handler_181
.word $__int_handler_182
.word $__int_handler_183
.word $__int_handler_184
.word $__int_handler_185
.word $__int_handler_186
.word $__int_handler_187
.word $__int_handler_188
.word $__int_handler_189
.word $__int_handler_190
.word $__int_handler_191
.word $__int_handler_192
.word $__int_handler_193
.word $__int_handler_194
.word $__int_handler_195
.word $__int_handler_196
.word $__int_handler_197
.word $__int_handler_198
.word $__int_handler_199
.word $__int_handler_200
.word $__int_handler_201
.word $__int_handler_202
.word $__int_handler_203
.word $__int_handler_204
.word $__int_handler_205
.word $__int_handler_206
.word $__int_handler_207
.word $__int_handler_208
.word $__int_handler_209
.word $__int_handler_210
.word $__int_handler_211
.word $__int_handler_212
.word $__int_handler_213
.word $__int_handler_214
.word $__int_handler_215
.word $__int_handler_216
.word $__int_handler_217
.word $__int_handler_218
.word $__int_handler_219
.word $__int_handler_220
.word $__int_handler_221
.word $__int_handler_222
.word $__int_handler_223
.word $__int_handler_224
.word $__int_handler_225
.word $__int_handler_226
.word $__int_handler_227
.word $__int_handler_228
.word $__int_handler_229
.word $__int_handler_230
.word $__int_handler_231
.word $__int_handler_232
.word $__int_handler_233
.word $__int_handler_234
.word $__int_handler_235
.word $__int_handler_236
.word $__int_handler_237
.word $__int_handler_238
.word $__int_handler_239
.word $__int_handler_240
.word $__int_handler_241
.word $__int_handler_242
.word $__int_handler_243
.word $__int_handler_244
.word $__int_handler_245
.word $__int_handler_246
.word $__int_handler_247
.word $__int_handler_248
.word $__int_handler_249
.word $__int_handler_250
.word $__int_handler_251
.word $__int_handler_252
.word $__int_handler_253
.word $__int_handler_254
.word $__int_handler_255
