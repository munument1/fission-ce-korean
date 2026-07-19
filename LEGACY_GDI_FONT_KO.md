# 레거시 한국어 GDI 폰트 렌더러

이 작업 사본에는 과거 한국어 패치의 `fallout2font.dll` 동작을 F.I.S.S.I.O.N. 소스로 옮긴 Windows 전용 렌더러가 들어 있다.

## 사용 방법

`fission.cfg`의 `[font]` 섹션에 다음 값을 추가한다.

```ini
[font]
gdi_renderer=1
ttf_renderer=0
legacy_codepage=949
gdi_text_face=Dotum
gdi_text_size=11
gdi_text_weight=400
gdi_text_line_height=11
gdi_button_face=NanumBarunGothic
gdi_button_size=15
gdi_button_weight=400
gdi_button_line_height=17
gdi_title_face=NanumBarunGothic
gdi_title_size=18
gdi_title_weight=400
gdi_title_line_height=20
gdi_binary_threshold=128
baseline_offset=0
```

전체 예시는 `GDI_FONT_PRESET.cfg`에 있다.

## 렌더링 방식

1. CP949 텍스트를 UTF-16으로 변환한다.
2. Windows GDI의 `CreateFontW`와 `TextOutW`로 32bpp DIBSection에 출력한다.
3. DIB 픽셀의 밝기가 `gdi_binary_threshold` 이상이면 Fallout 8비트 화면 버퍼에 요청된 팔레트 색을 기록한다.
4. font 100/101은 일반, 102는 큰 글꼴, 103은 굵은 중간 글꼴, 104는 제목 글꼴로 나눠 사용한다.

CR/LF 같은 개행 제어문자는 GDI에 전달하지 않으므로 줄 끝에 네모 글리프가 나타나지 않는다.

기본 임계값 1은 과거 DLL처럼 검정이 아닌 픽셀을 모두 단색 획으로 바꾼다. 글자가 너무 굵으면 32, 64, 96 순서로 시험한다.

## 기존 렌더러와의 관계

- `gdi_renderer=1`: Windows GDI 경로를 강제로 사용한다.
- `gdi_renderer=0`: Windows에서 `language=korean`이면 GDI 경로를 자동 사용한다.
- `gdi_renderer=-1`: GDI 경로를 강제로 끈다.
- `gdi_renderer=0`, `ttf_renderer=1`: 기존 stb_truetype TTF 경로를 사용한다.
- 둘 다 0: 원본 FON/AAF 렌더러를 사용한다.
- Windows 이외의 플랫폼에서는 GDI 경로가 자동으로 비활성화된다.

## 구현 파일

- `src/legacy_gdi_font.cc`
- `src/legacy_gdi_font.h`
- `src/korean_font.cc`
- `src/settings.h`
- `src/settings.cc`
- `src/game_config.h`

## 현재 상태

Windows x64 Debug 빌드가 완료되었다. 실제 게임 화면 검증에는 Fallout 2 데이터와 CP949 한국어 텍스트가 들어 있는 시험 설치본이 필요하다.
