#ifndef PICO_SKYACE_PLATFORM_HIGH_SCORE_H_
#define PICO_SKYACE_PLATFORM_HIGH_SCORE_H_

#include <cstdint>

// ハイスコアはゲーム本体からストレージの詳細を隠して扱う。SDカードが
// 使えない場合も、RAM上のスコア更新は成功させてプレイを継続する。
namespace skyace::high_score {

// 起動時にSDカード上の保存記録を読み込む。SDなし・未フォーマット・破損
// ファイルは空記録として扱い、ゲームを止めない。
void init();

// 現在のハイスコア。init()前は0を返す。
int current();

// scoreが現在値を上回る場合にRAM値を更新し、SDカードへ保存を試みる。
// 新記録ならtrue。保存に失敗しても新記録自体はセッション中保持する。
bool submit(int score);

}  // namespace skyace::high_score

#endif  // PICO_SKYACE_PLATFORM_HIGH_SCORE_H_
