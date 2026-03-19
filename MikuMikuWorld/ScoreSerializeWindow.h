#pragma once
#include "ScoreSerializer.h"
#include "ScoreContext.h"

namespace MikuMikuWorld
{
	class DefaultScoreSerializeController : public ScoreSerializeController
	{
		void createSerializer();

	  public:
		DefaultScoreSerializeController(SerializingScore score);
		DefaultScoreSerializeController(SerializingScore score, const std::string& filename);

		SerializeResult update() override;

	  private:
		std::unique_ptr<ScoreSerializer> serializer;
		std::vector<Result> serializable;
		SerializeFormat selectedFormat{ -1 };
	};

	class DefaultScoreDeserializeController : public ScoreSerializeController
	{
		void createDeserializer();

	  public:
		DefaultScoreDeserializeController(const std::string& filename);

		SerializeResult update() override;

	  private:
		std::unique_ptr<ScoreSerializer> deserializer;
		SerializeFormat selectedFormat{ -1 };
	};

	class ScoreEditor;
	class ScoreEditorTimeline;
	class ScoreSerializeWindow
	{
	  public:
		ScoreSerializeWindow() = default;
		void update(ScoreEditor& editor);
		void serialize(ScoreEditorTimeline& timeline);
		void serialize(ScoreEditorTimeline& timeline, const std::string& filename);
		void deserialize(ScoreEditorTimeline& timeline, const std::string& filename);
		bool isSerializing() const;

	  private:
		std::unique_ptr<ScoreSerializeController> controller{};
		ScoreEditorTimeline* timeline{};
	};
}