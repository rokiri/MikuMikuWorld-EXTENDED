task "build:msbuild" do
  puts "Running msbuild"
  sh "msbuild /p:Configuration=Release /p:Platform=x64"
end

task "build:copy" do
  require "fileutils"

  puts "Building build/"

  FileUtils.rm_rf "build"
  FileUtils.mkdir_p "build"
  FileUtils.cp_r "x64/Release", "build/MikuMikuWorld"
  FileUtils.cp "build/MikuMikuWorld/MikuMikuWorld.pdb",
               "build/__YOU_DONT_NEED_THIS__.pdb"
  FileUtils.rm "build/MikuMikuWorld/MikuMikuWorld.pdb"
end

task "build:installer" do
  puts "Building installer"
  installer = File.read("./installerBase.nsi")
  version =
    File
      .read("./MikuMikuWorld/MikuMikuWorld.rc")
      .match(/FILEVERSION (\d+),(\d+),(\d+),(\d+)/)
      .to_a[
      1..-1
    ].join(".")
  File.write("./installer.nsi", installer.gsub(/{version}/, version))

  candidates = []
  exts = ENV["PATHEXT"]&.split(";") || [""]
  ENV["PATH"].to_s.split(File::PATH_SEPARATOR).each do |dir|
    exts.each do |ext|
      candidates << File.join(dir, "makensis#{ext}")
    end
  end

  candidates << "C:/Program Files (x86)/NSIS/makensis.exe"
  candidates << "C:/Program Files/NSIS/makensis.exe"

  makensis = candidates.find { |p| File.file?(p) }

  abort "makensis not found" unless makensis

  sh %("#{makensis}" installer.nsi)
end

task "build:zip" do
  puts "Building zip"
  sh "7z a -tzip MikuMikuWorld.zip MikuMikuWorld", { chdir: "build" }
end

task "build" => %w[build:msbuild build:copy build:installer build:zip]

task "check:translation" do
  def list_keys(file)
    file
      .split("\n")
      .filter_map do |line|
        next nil if line.start_with?("#")
        next nil unless line.include?(",")
        line.split(",")[0]
      end
  end

  files = Dir.children("./MikuMikuWorld/res/i18n")
  template = File.read("./MikuMikuWorld/res/i18n/.template.csv")
  template_keys = list_keys(template)

  coverages = {}
  files.each do |file|
    next if file == ".template.csv"

    puts "== #{file}"
    keys = list_keys(File.read("./MikuMikuWorld/res/i18n/#{file}"))
    missing_keys = template_keys - keys
    if missing_keys.size > 0
      puts "missing keys:"
      puts "  " + missing_keys.join(", ")
    end
    coverage =
      ((1 - missing_keys.size.to_f / template_keys.size) * 100).round(2)
    puts "Coverage: #{coverage}%"
    coverages[file.split(".")[0]] = coverage
  end
  if (output_file = ENV["GITHUB_OUTPUT"])
    puts "Writing to GITHUB_OUTPUT"
    pp coverages
    coverages.each do |lang, coverage|
      File.write(output_file, "coverage_#{lang}=#{coverage}\n", mode: "a+")
    end
  end
end

task "check" => %w[check:translation]

task "update", [:version] do |t, args|
  abort "version is required" unless args[:version]
  rc = File.read("./MikuMikuWorld/MikuMikuWorld.rc")

  version_raw = args[:version]
  version = version_raw.sub(/[^0-9.].*/, "").split(".")
  puts "Update: v#{version_raw} (#{version.join(".")})"
  rc.gsub!(/FILEVERSION .+/, "FILEVERSION #{version.join(",")}")
  rc.gsub!(/PRODUCTVERSION .+/, "PRODUCTVERSION #{version.join(",")}")
  rc.gsub!(/"FileVersion", ".+"/, %Q("FileVersion", "#{version.join(".")}"))
  rc.gsub!(
    /"ProductVersion", ".+"/,
    %Q("ProductVersion", "#{version.join(".")}")
  )
  File.write("./MikuMikuWorld/MikuMikuWorld.rc", rc)

  changelog_path = ".update-changelog"
  changelog = File.exist?(changelog_path) ? File.read(changelog_path).strip : ""

  commit_message = if changelog.empty?
    "release: v#{version_raw}"
  else
    <<~MSG.strip
      release: v#{version_raw}

      #{changelog}
    MSG
  end

  sh %Q(git commit --allow-empty -am "#{commit_message.gsub('"', '\"')}")
  sh %Q(git tag -f v#{version_raw})
end

task "action:version" do
  require "open3"

  ref = ENV["GITHUB_REF"] or abort "GITHUB_REF not set"
  tag = ref.split("/").last

  rc = File.read("./MikuMikuWorld/MikuMikuWorld.rc")
  version_raw = rc.match(/FILEVERSION\s+(\d+),(\d+),(\d+),(\d+)/).captures.join(".")

  if tag.include?("-preview")
    body, status = Open3.capture2("git", "log", "-1", "--pretty=%b")
    abort "git log failed" unless status.success?

    prerelease = "true"
    release_body = body.strip
  else
    tags, status = Open3.capture2("git", "tag", "--sort=-creatordate")
    abort "git tag failed" unless status.success?

    prev_tag = tags
    .lines
    .map(&:strip)
    .reject { |t| t.start_with?(tag) || t.match?("v.+-.+") }
    .first or ""

    prerelease = "false"
    release_body = <<~BODY
      Download "mmw4uc-#{version_raw}-setup.exe" or "MikuMikuWorld.zip".

      **Full Changelog**: https://github.com/UntitledCharts/MikuMikuWorld4UC/compare/#{prev_tag}...#{tag}
    BODY
  end

  github_output = ENV["GITHUB_OUTPUT"] or abort "GITHUB_OUTPUT not set"

  File.open(github_output, "a") do |f|
    f.puts "prerelease=#{prerelease}"
    f.puts "body<<EOF"
    f.puts release_body
    f.puts "EOF"
  end
end

desc "Convert i18n files to YAML format"
task "convert_locales_yaml" do
  input_dir  = "./MikuMikuWorld/res/i18n"
  output_dir = "./MikuMikuWorld/Localization"
  base_lang = "en"

  FileUtils.mkdir_p(output_dir)

  Dir.glob(File.join(input_dir, "*.csv"), File::FNM_DOTMATCH).each do |file_path|
    lines = File.readlines(file_path, encoding: "utf-8")

    lang_code = File.basename(file_path, ".csv")

    if lines.empty?
      puts "Skipping empty file: #{file_path}"
      next
    end

    output = []

    lines.each do |line|
      raw = line.chomp

      # Preserve empty line
      if raw.empty?
        output << ""
        next
      end

      # Missing translation
      if raw.lstrip.start_with?("#- ")
        content = raw.lstrip[3..] # remove "#- "
        
        key, rest = content.split(",", 2)
      
        if key
          key = key # preserve spacing as-is
          yaml_line = "#{key}: "
          output << yaml_line
        else
          output << raw # fallback safety
        end
      
        next
      end

      # Comment
      if raw.lstrip.start_with?("#")
        output << raw
        next
      end

      # Split ONLY first comma, preserve spaces
      key, rest = raw.split(",", 2)

      if rest.nil?
        output << raw # fallback: keep as-is
        next
      end

      value_part = rest

      # Split inline comment safely (preserve spaces before #)
      val, comment = value_part.split("#", 2)

      value = val || ""

      # Determine if quoting is needed
      needs_quote =
        value.start_with?(" ") ||
        value.end_with?(" ") ||
        value.include?(":") ||
        value.include?("#") ||
        value.include?('"') ||
        value.include?('%') ||
        value.include?('@') ||
        value.include?("\n")

      if needs_quote
        escaped = value.gsub('"', '\"')
        value = "\"#{escaped}\""
      end

      yaml_line = "#{key}: #{value}"

      if comment
        yaml_line += "##{comment}"  # keep original spacing
      end

      output << yaml_line
    end

    output_filename = if base_lang != lang_code then
      "#{base_lang}@#{lang_code}.yaml"
    else
      "#{lang_code}.yaml"
    end

    output_file = File.join(
      output_dir,
      output_filename
    )

    File.write(output_file, output.join("\n"), encoding: "utf-8")

    puts "Converted: #{file_path} -> #{output_file}"
  end
end

desc "Convert YAML i18n files back to CSV format"
task "convert_locales_csv" do
  input_dir  = "./MikuMikuWorld/Localization"
  output_dir = "./MikuMikuWorld/res/i18n"
  base_lang = "en"

  FileUtils.mkdir_p(output_dir)

  Dir.glob(File.join(input_dir, "*.yaml"), File::FNM_DOTMATCH).each do |file_path|
    filename = File.basename(file_path)

    # Match: en@ru.yaml
    match = filename.match(/^#{base_lang}@([A-Za-z0-9_-]+)\.yaml$/)

    unless match
      puts "Skipping non-matching file: #{filename}"
      next
    end

    is_template = File.basename(file_path).include?(".template")
    lines = File.readlines(file_path, encoding: "utf-8")

    if lines.empty?
      puts "Skipping empty file: #{file_path}"
      next
    end

    lang_code = match[1]

    output = []

    lines.each do |line|
      raw = line.chomp

      # Empty
      if raw.strip.empty?
        output << ""
        next
      end

      # Comment
      if raw.lstrip.start_with?("#")
        output << raw
        next
      end

      # Remove indentation ONLY (not inner spaces)
      content = raw.sub(/^\s+/, '')

      key, rest = content.split(": ", 2)

      if rest.nil?
        output << raw
        next
      end

      value_part = rest

      # Split inline comment
      val, comment = value_part.split("#", 2)

      value = val || ""

      if value == "" && !is_template
        csv_line = "#- #{key},"
        output << csv_line
        next
      end

      # Remove wrapping quotes ONLY if we added them
      if value.start_with?('"') && value.end_with?('"')
        inner = value[1..-2]
        value = inner.gsub('\"', '"')
      end

      csv_line = "#{key},#{value}"

      if comment
        csv_line += "##{comment}"
      end

      output << csv_line
    end

    output_file = File.join(
      output_dir,
      "#{lang_code}.csv"
    )

    File.write(output_file, output.join("\n"), encoding: "utf-8")

    puts "Converted: #{file_path} -> #{output_file}"
  end
end