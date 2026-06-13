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

      Full Changelog: #{prev_tag}..#{tag}
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
