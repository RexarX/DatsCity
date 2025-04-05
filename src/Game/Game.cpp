#include "Game.h"
#include "Application.h"

namespace app {

bool CompareLength(const std::string& a, const std::string& b) {
  return a.length() > b.length();
}

int CountDigits(int number) {
  int answer = 0;

  while (number > 0) {
    ++answer;
    number /= 10;
  }

  return answer;
}

void Game::Update(const GameState& game_state) {
  Application& app = Application::Get();
  
  if (current_turn_ != app.GetCurrentWords().turn) {
    UpdateInfo(app);
  }
  
}

void Game::UpdateInfo(Application& app) {
  current_turn_ = app.GetCurrentWords().turn;

  words_ = app.GetCurrentWords().words;
  std::sort(words_.begin(), words_.end(), CompareLength);

  if (words_.size() == 0) {
    INFO("No words found");
    return;
  }

  CountWordSizes();

  PrintWords("Words.txt");
  PrintStatistics("Stats.txt");
}

void Game::CountWordSizes() {
  word_sizes_.clear();
  word_sizes_.resize(10, 0);
  // counting words by their sizes
  for (const auto& word : words_) {
    size_t visual_size = word.size() / 2;
    if (visual_size > word_sizes_.size() - 1) {
      word_sizes_.resize(visual_size + 1, 0);
    }
    word_sizes_[visual_size]++;
  }
}

void Game::PrintWords(std::string_view words_filename) {
  std::ofstream words_file(words_filename.data());

  // listing words by their sizes
  for (const auto& word : words_) {
    size_t visual_size = word.size() / 2;

    words_file << visual_size << (visual_size < 10 ? "  " : " ") << word << '\n';
  }

  INFO("Words printed to {}", words_filename);
}

void Game::PrintStatistics(std::string_view stats_filename) {

  //calculate max size
  size_t max_size = word_sizes_.size() - 1;
  int max_size_digits = CountDigits(max_size);

  //calculate max count
  int max_count = 0;
  for (size_t count : word_sizes_) {
    if (count > max_count) {
      max_count = count;
    }
  }
  int max_count_digits = CountDigits(max_count);

  std::ofstream stat_file(stats_filename.data());

  //printing table names
  for (int i = 0; i < max_size_digits + 1; ++i) {
    stat_file << ' ';
  }
  for (int i = 0; i < 10; ++i) {
    stat_file << i;
    for (int j = 0; j < max_count_digits; j++) {
      stat_file << ' ';
    }
  }
  stat_file << '\n';
  
  //filling the tables
  for (int i = 0; i <= max_size / 10; ++i) {
    stat_file << i * 10;
    int decades = (i == 0 ? 1 : i * 10);
    for (int j = 0; j < max_size_digits - CountDigits(decades) + 1; ++j) {
      stat_file << ' ';
    }

    for (int j = i * 10; j < (i + 1) * 10 && j <= max_size; ++j) {
      stat_file << word_sizes_[j];
      int word_size = (word_sizes_[j] == 0 ? 1 : word_sizes_[j]);
      for (int k = 0; k < max_count_digits - CountDigits(word_size) + 1; ++k) {
        stat_file << ' ';
      }
    }

    stat_file << '\n';
  }

  INFO("Stats printed to {}", stats_filename);
}

}  // namespace app