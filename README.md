# NTAP-C

NTAP-C 鏄?NTAP 鐨勫鎴风銆俉indows 绔潰鍚戞櫘閫氱敤鎴锋彁渚涘浘褰㈢晫闈細鍙屽嚮 `ntap-c.exe`锛岃緭鍏ヨ繛鎺ヤ俊鎭紝鐐瑰嚮 Connect 鍚庤嚜鍔ㄥ啓鍏ラ厤缃€佹鏌?TAP锛屽苟鍦ㄩ渶瑕佹椂鎷夎捣绠＄悊鍛樻潈闄愬噯澶?TAP 閫傞厤鍣ㄣ€?
Linux 绔繚鐣欏懡浠よ鍏ュ彛锛岄€傚悎鏈嶅姟鍣ㄣ€佹祴璇曠幆澧冨拰鑷姩鍖栭儴缃层€?
## 涓変釜浠撳簱

NTAP 鎷嗘垚涓変釜骞插噣鐨勬簮鐮佷粨搴擄紝鏈€缁堝彲閮ㄧ讲鏂囦欢缁熶竴鏀惧湪鍚勮嚜 GitHub Release锛?
- [NTAP-A](https://github.com/VAMPIRE0924/NTAP-A): 鍏綉鏈嶅姟绔紝璐熻矗绠＄悊 API銆丼QLite 鐘舵€佸簱銆佽妭鐐?TAP 閴存潈銆乀apHub 涓户銆?- [NTAP-B](https://github.com/VAMPIRE0924/NTAP-B): 鑺傜偣绔紝閮ㄧ讲鍦ㄥ鎴蜂晶缃戝叧鎴栧唴缃戜富鏈猴紝杩炴帴 A 骞舵帴鍏ユ湰鍦扮綉缁溿€?- [NTAP-C](https://github.com/VAMPIRE0924/NTAP-C): 瀹㈡埛绔紝Windows 绔彁渚涘浘褰㈢晫闈紝Linux 绔彁渚涘懡浠よ鍏ュ彛銆?
## 涓嬭浇鍜岄儴缃?
姝ｅ紡閮ㄧ讲璇蜂笅杞?GitHub Release 閲岀殑鏈€缁堝彂甯冨寘锛屼笉瑕佺洿鎺ユ嬁婧愮爜鐩綍閲岀殑涓存椂鏂囦欢閮ㄧ讲銆?
鏈€鏂扮増鏈細

https://github.com/VAMPIRE0924/NTAP-C/releases/latest

Windows 瀹㈡埛绔笅杞斤細

```text
NTAP-C-<version>-windows-x64.zip
```

瑙ｅ帇鍚庡鎴峰彧闇€瑕佽繍琛岋細

```text
bin\ntap-c.exe
```

绐楀彛閲屽～鍐欙細

- NTAP-A 鍦板潃
- TAP 鐢ㄦ埛鍚?- TAP 瀵嗙爜
- Network ID
- TAP 閫傞厤鍣ㄥ悕绉帮紝榛樿 `ntap-c0`

鐐瑰嚮 Connect 鍚庯紝GUI 浼氳皟鐢ㄥ悓鐩綍鐨?`ntap-c-cli.exe` 鎵ц瀹為檯杩炴帴銆俙ntap-c-cli.exe` 涓嶉潰鍚戞櫘閫氬鎴凤紝鍙敤浜庢牎楠屻€佽嚜鍔ㄥ寲鍜屾湇鍔″寲閮ㄧ讲銆?
## Windows TAP

褰撳墠 Windows 鏁版嵁闈㈡敮鎸?TAP-Windows6/OpenVPN 椋庢牸閫傞厤鍣ㄣ€俉intun/WireGuard 閫傞厤鍣ㄧ洰鍓嶅彧鍋氬彂鐜帮紝鍚庣画闇€瑕佸崟鐙殑鏁版嵁闈㈠悗绔€?
濡傛灉瀹㈡埛鏈哄櫒娌℃湁 TAP-Windows6锛孏UI 浼氬皾璇曡皟鐢?Release 鍖呴噷鐨勶細

```text
install\ensure-tap-windows.ps1
```

璇ヨ剼鏈渶瑕佺鐞嗗憳鏉冮檺锛屽苟浼氬湪绯荤粺宸叉湁 OpenVPN TAP 宸ュ叿鎴栭┍鍔ㄦ枃浠舵椂鍒涘缓/鍑嗗 TAP 閫傞厤鍣ㄣ€傛病鏈夐┍鍔ㄦ椂锛岄渶瑕佸厛瀹夎 OpenVPN TAP-Windows6 椹卞姩銆?
## Linux 瀹㈡埛绔?
Linux 瀹㈡埛绔笅杞斤細

```text
NTAP-C-<version>-linux-x64.tar.gz
```

鍩烘湰鍛戒护锛?
```sh
bin/ntap-c -c conf/ntap-c.conf.example check-env
bin/ntap-c -c conf/ntap-c.conf.example run
```

Linux 绔渶瑕?`/dev/net/tun` 鍜屽垱寤?TAP 鐨勬潈闄愩€?
## 婧愮爜鑼冨洿

```text
src/c/       NTAP-C 瀹㈡埛绔簮鐮?src/common/  涓夌鍏变韩鍗忚銆佹棩蹇椼€佺綉缁溿€佹椂闂淬€乥uffer 绛夊叕鍏变唬鐮?conf/        鏈€灏忛厤缃ず渚?```

婧愮爜浠撳簱鍙繚瀛樻簮鐮併€侀厤缃牱渚嬨€丷EADME 鍜?LICENSE锛涙渶缁堝彂甯冨寘鍙斁鍦?GitHub Release銆?
## License

GPL-3.0-only. See `LICENSE`.

